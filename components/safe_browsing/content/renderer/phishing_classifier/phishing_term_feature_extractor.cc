// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_term_feature_extractor.h"

#include <list>
#include <map>
#include <memory>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/murmurhash3_util.h"
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

  ExtractionState(const std::u16string& text, base::TimeTicks start_time_ticks)
      : start_time(start_time_ticks), num_iterations(0) {
    std::unique_ptr<base::i18n::BreakIterator> i(new base::i18n::BreakIterator(
        text, base::i18n::BreakIterator::BREAK_WORD));

    if (i->Init())
      iterator = std::move(i);
  }
};

PhishingTermFeatureExtractor::PhishingTermFeatureExtractor(
    base::RepeatingCallback<bool(const std::string&)> find_page_term_callback,
    base::RepeatingCallback<bool(uint32_t)> find_page_word_callback,
    size_t max_words_per_term,
    uint32_t murmurhash3_seed,
    size_t max_shingles_per_page,
    size_t shingle_size)
    : find_page_term_callback_(find_page_term_callback),
      find_page_word_callback_(find_page_word_callback),
      max_words_per_term_(max_words_per_term),
      murmurhash3_seed_(murmurhash3_seed),
      max_shingles_per_page_(max_shingles_per_page),
      shingle_size_(shingle_size),
      clock_(base::DefaultTickClock::GetInstance()) {
  Clear();
}

PhishingTermFeatureExtractor::~PhishingTermFeatureExtractor() {
  CancelPendingExtraction();
}

void PhishingTermFeatureExtractor::ExtractFeatures(
    const std::u16string* page_text,
    FeatureMap* features,
    std::set<uint32_t>* shingle_hashes,
    DoneCallback done_callback) {
  // The RenderView should have called CancelPendingExtraction() before
  // starting a new extraction, so DCHECK this.
  DCHECK(done_callback_.is_null());
  DCHECK(!state_.get());
  // However, in an opt build, we will go ahead and clean up the pending
  // extraction so that we can start in a known state.
  CancelPendingExtraction();

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("safe_browsing", "ExtractTermFeatures",
                                    this);

  page_text_ = page_text;
  features_ = features;
  shingle_hashes_ = shingle_hashes, done_callback_ = std::move(done_callback);

  state_ = std::make_unique<ExtractionState>(*page_text_, clock_->NowTicks());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
  base::TimeTicks current_chunk_start_time = clock_->NowTicks();

  if (!state_->iterator.get()) {
    // We failed to initialize the break iterator, so stop now.
    RunCallback(false);
    return;
  }

  int num_words = 0;
  while (state_->iterator->Advance()) {
    if (state_->iterator->IsWord()) {
      const size_t start = state_->iterator->prev();
      const size_t length = state_->iterator->pos() - start;
      HandleWord(std::u16string_view(*page_text_).substr(start, length));
      ++num_words;
    }

    if (num_words >= kClockCheckGranularity) {
      num_words = 0;
      base::TimeTicks now = clock_->NowTicks();
      if (now - state_->start_time >= base::Milliseconds(kMaxTotalTimeMs)) {
        RunCallback(false);
        return;
      }
      base::TimeDelta chunk_elapsed = now - current_chunk_start_time;
      if (chunk_elapsed >= base::Milliseconds(kMaxTimePerChunkMs)) {
        // The time limit for the current chunk is up, so post a task to
        // continue extraction.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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

void PhishingTermFeatureExtractor::HandleWord(std::u16string_view word) {
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
  if (!find_page_word_callback_.Run(word_hash)) {
    // Word doesn't exist in our terms so we can clear the n-gram state.
    state_->previous_words.clear();
    state_->previous_word_sizes.clear();
    return;
  }

  // Find all of the n-grams that we need to check and compute their SHA-256
  // hashes.
  std::map<std::string /* hash */, std::string /* plaintext */> hashes_to_check;
  hashes_to_check[crypto::SHA256HashString(word_lower)] = word_lower;

  // Combine the new word with the previous words to find additional n-grams.
  // Note that we don't yet add the new word length to previous_word_sizes,
  // since we don't want to compute the hash for the word by itself again.
  //
  state_->previous_words.append(word_lower);
  std::string current_term = state_->previous_words;
  for (const auto& previous_word_size : state_->previous_word_sizes) {
    hashes_to_check[crypto::SHA256HashString(current_term)] = current_term;
    current_term.erase(0, previous_word_size);
  }

  // Add features for any hashes that match page_term_hashes_.
  for (const auto& hash_to_check : hashes_to_check) {
    if (find_page_term_callback_.Run(hash_to_check.first)) {
      features_->AddBooleanFeature(features::kPageTerm + hash_to_check.second);
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

void PhishingTermFeatureExtractor::RunCallback(bool success) {
  // Record some timing stats that we can use to evaluate feature extraction
  // performance.  These include both successful and failed extractions.
  DCHECK(state_.get());

  DCHECK(!done_callback_.is_null());
  TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "ExtractTermFeatures", this);
  std::move(done_callback_).Run(success);
  Clear();
}

void PhishingTermFeatureExtractor::Clear() {
  page_text_ = nullptr;
  features_ = nullptr;
  shingle_hashes_ = nullptr;
  done_callback_.Reset();
  state_.reset(nullptr);
}

}  // namespace safe_browsing
