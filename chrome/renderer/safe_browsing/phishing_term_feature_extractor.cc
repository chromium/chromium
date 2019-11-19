// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_term_feature_extractor.h"

#include <list>
#include <map>
#include <memory>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/renderer/safe_browsing/feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/murmurhash3_util.h"
#include "crypto/sha2.h"

namespace safe_browsing {

// This time should be short enough that it doesn't noticeably disrupt the
// user's interaction with the page.
const int PhishingTermFeatureExtractor::kMaxTimePerChunkMs = 10;

// Experimenting shows that we get a reasonable gain in performance by
// increasing this up to around 10, but there's not much benefit in
// increasing it past that.
const int PhishingTermFeatureExtractor::kClockCheckGranularity = 5;

// This should be longer than we expect feature extraction to take on any
// actual phishing page.
const int PhishingTermFeatureExtractor::kMaxTotalTimeMs = 500;

// All of the state pertaining to the current feature extraction.
struct PhishingTermFeatureExtractor::ExtractionState {
  // Stores up to max_words_per_term_ previous words separated by spaces.
  std::string previous_words;

  // Stores the current shingle after a new word is processed and added in.
  std::string current_shingle;

  // Stores the sizes of the words in current_shingle. Note: the size includes
  // the space after each word. In other words, the sum of all sizes in this
  // list is equal to the length of current_shingle.
  std::list<size_t> shingle_word_sizes;

  // Stores the sizes of the words in previous_words.  Note: the size includes
  // the space after each word.  In other words, the sum of all sizes in this
  // list is equal to the length of previous_words.
  std::list<size_t> previous_word_sizes;

  // An iterator for word breaking.
  std::unique_ptr<base::i18n::BreakIterator> iterator;

  // The time at which we started feature extraction for the current page.
  base::TimeTicks start_time;

  // The number of iterations we've done for the current extraction.
  int num_iterations;

  ExtractionState(const base::string16& text, base::TimeTicks start_time_ticks)
      : start_time(start_time_ticks),
        num_iterations(0) {
    std::unique_ptr<base::i18n::BreakIterator> i(new base::i18n::BreakIterator(
        text, base::i18n::BreakIterator::BREAK_WORD));

    if (i->Init()) {
      iterator = std::move(i);
    } else {
      DLOG(ERROR) << "failed to open iterator";
    }
  }
};

PhishingTermFeatureExtractor::PhishingTermFeatureExtractor(
    const std::unordered_set<std::string>* page_term_hashes,
    const std::unordered_set<uint32_t>* page_word_hashes,
    size_t max_words_per_term,
    uint32_t murmurhash3_seed,
    size_t max_shingles_per_page,
    size_t shingle_size,
    FeatureExtractorClock* clock)
    : page_term_hashes_(page_term_hashes),
      page_word_hashes_(page_word_hashes),
      max_words_per_term_(max_words_per_term),
      murmurhash3_seed_(murmurhash3_seed),
      max_shingles_per_page_(max_shingles_per_page),
      shingle_size_(shingle_size),
      clock_(clock) {
  Clear();
}

PhishingTermFeatureExtractor::~PhishingTermFeatureExtractor() {
  // The RenderView should have called CancelPendingExtraction() before
  // we are destroyed.
  CheckNoPendingExtraction();
}

void PhishingTermFeatureExtractor::ExtractFeatures(
    const base::string16* page_text,
    FeatureMap* features,
    std::set<uint32_t>* shingle_hashes,
    DoneCallback done_callback) {
  // The RenderView should have called CancelPendingExtraction() before
  // starting a new extraction, so DCHECK this.
  CheckNoPendingExtraction();
  // However, in an opt build, we will go ahead and clean up the pending
  // extraction so that we can start in a known state.
  CancelPendingExtraction();

  page_text_ = page_text;
  features_ = features;
  shingle_hashes_ = shingle_hashes, done_callback_ = std::move(done_callback);

  state_.reset(new ExtractionState(*page_text_, clock_->Now()));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PhishingTermFeatureExtractor::ExtractFeaturesWithTimeout,
                     weak_factory_.GetWeakPtr()));
}

void PhishingTermFeatureExtractor::CancelPendingExtraction() {
  // Cancel any pending callbacks, and clear our state.
  weak_factory_.InvalidateWeakPtrs();
  Clear();
}

void PhishingTermFeatureExtractor::ExtractFeaturesWithTimeout() {
  DCHECK(state_.get());
  ++state_->num_iterations;
  base::TimeTicks current_chunk_start_time = clock_->Now();

  if (!state_->iterator.get()) {
    // We failed to initialize the break iterator, so stop now.
    UMA_HISTOGRAM_COUNTS_1M("SBClientPhishing.TermFeatureBreakIterError", 1);
    RunCallback(false);
    return;
  }

  int num_words = 0;
  while (state_->iterator->Advance()) {
    if (state_->iterator->IsWord()) {
      const size_t start = state_->iterator->prev();
      const size_t length = state_->iterator->pos() - start;
      HandleWord(base::StringPiece16(page_text_->data() + start, length));
      ++num_words;
    }

    if (num_words >= kClockCheckGranularity) {
      num_words = 0;
      base::TimeTicks now = clock_->Now();
      if (now - state_->start_time >=
          base::TimeDelta::FromMilliseconds(kMaxTotalTimeMs)) {
        DLOG(ERROR) << "Feature extraction took too long, giving up";
        // We expect this to happen infrequently, so record when it does.
        UMA_HISTOGRAM_COUNTS_1M("SBClientPhishing.TermFeatureTimeout", 1);
        RunCallback(false);
        return;
      }
      base::TimeDelta chunk_elapsed = now - current_chunk_start_time;
      if (chunk_elapsed >=
          base::TimeDelta::FromMilliseconds(kMaxTimePerChunkMs)) {
        // The time limit for the current chunk is up, so post a task to
        // continue extraction.
        //
        // Record how much time we actually spent on the chunk.  If this is
        // much higher than kMaxTimePerChunkMs, we may need to adjust the
        // clock granularity.
        UMA_HISTOGRAM_TIMES("SBClientPhishing.TermFeatureChunkTime",
                            chunk_elapsed);
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &PhishingTermFeatureExtractor::ExtractFeaturesWithTimeout,
                weak_factory_.GetWeakPtr()));
        return;
      }
      // Otherwise, continue.
    }
  }
  RunCallback(true);
}

void PhishingTermFeatureExtractor::HandleWord(
    const base::StringPiece16& word) {
  // First, extract shingle hashes.
  const std::string& word_lower = base::UTF16ToUTF8(base::i18n::ToLower(word));
  state_->current_shingle.append(word_lower + " ");
  state_->shingle_word_sizes.push_back(word_lower.size() + 1);
  if (state_->shingle_word_sizes.size() == shingle_size_) {
    shingle_hashes_->insert(
        MurmurHash3String(state_->current_shingle, murmurhash3_seed_));
    state_->current_shingle.erase(0, state_->shingle_word_sizes.front());
    state_->shingle_word_sizes.pop_front();
  }
  // Check if the size of shingle hashes is over the limit.
  if (shingle_hashes_->size() > max_shingles_per_page_) {
    // Pop the largest one.
    auto it = shingle_hashes_->end();
    shingle_hashes_->erase(--it);
  }

  // Next, extract page terms.
  uint32_t word_hash = MurmurHash3String(word_lower, murmurhash3_seed_);

  // Quick out if the word is not part of any term, which is the common case.
  if (page_word_hashes_->find(word_hash) == page_word_hashes_->end()) {
    // Word doesn't exist in our terms so we can clear the n-gram state.
    state_->previous_words.clear();
    state_->previous_word_sizes.clear();
    return;
  }

  // Find all of the n-grams that we need to check and compute their SHA-256
  // hashes.
  std::map<std::string /* hash */, std::string /* plaintext */>
      hashes_to_check;
  hashes_to_check[crypto::SHA256HashString(word_lower)] = word_lower;

  // Combine the new word with the previous words to find additional n-grams.
  // Note that we don't yet add the new word length to previous_word_sizes,
  // since we don't want to compute the hash for the word by itself again.
  //
  state_->previous_words.append(word_lower);
  std::string current_term = state_->previous_words;
  for (auto it = state_->previous_word_sizes.begin();
       it != state_->previous_word_sizes.end(); ++it) {
    hashes_to_check[crypto::SHA256HashString(current_term)] = current_term;
    current_term.erase(0, *it);
  }

  // Add features for any hashes that match page_term_hashes_.
  for (auto it = hashes_to_check.begin(); it != hashes_to_check.end(); ++it) {
    if (page_term_hashes_->find(it->first) != page_term_hashes_->end()) {
      features_->AddBooleanFeature(features::kPageTerm + it->second);
    }
  }

  // Now that we have handled the current word, we have to add a space at the
  // end of it, and add the new word's size (including the space) to
  // previous_word_sizes.  Note: it's possible that the document language
  // doesn't use ASCII spaces to separate words.  That's fine though, we just
  // need to be consistent with how the model is generated.
  state_->previous_words.append(" ");
  state_->previous_word_sizes.push_back(word_lower.size() + 1);

  // Cap the number of previous words.
  if (state_->previous_word_sizes.size() >= max_words_per_term_) {
    state_->previous_words.erase(0, state_->previous_word_sizes.front());
    state_->previous_word_sizes.pop_front();
  }
}

void PhishingTermFeatureExtractor::CheckNoPendingExtraction() {
  DCHECK(done_callback_.is_null());
  DCHECK(!state_.get());
  if (!done_callback_.is_null() || state_.get()) {
    LOG(ERROR) << "Extraction in progress, missing call to "
               << "CancelPendingExtraction";
  }
}

void PhishingTermFeatureExtractor::RunCallback(bool success) {
  // Record some timing stats that we can use to evaluate feature extraction
  // performance.  These include both successful and failed extractions.
  DCHECK(state_.get());
  UMA_HISTOGRAM_COUNTS_1M("SBClientPhishing.TermFeatureIterations",
                          state_->num_iterations);
  UMA_HISTOGRAM_TIMES("SBClientPhishing.TermFeatureTotalTime",
                      clock_->Now() - state_->start_time);

  DCHECK(!done_callback_.is_null());
  std::move(done_callback_).Run(success);
  Clear();
}

void PhishingTermFeatureExtractor::Clear() {
  page_text_ = NULL;
  features_ = NULL;
  shingle_hashes_ = NULL;
  done_callback_.Reset();
  state_.reset(NULL);
}

}  // namespace safe_browsing
