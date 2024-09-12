// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/phrase_segmentation/phrase_segmenter.h"

#include <deque>
#include <tuple>
#include <vector>

#include "chrome/renderer/accessibility/phrase_segmentation/base_phrase_segmenter.h"
#include "chrome/renderer/accessibility/phrase_segmentation/token_boundaries.h"
#include "chrome/renderer/accessibility/phrase_segmentation/tokenized_sentence.h"

// Constants used in the phrase splitting algorithm. These were calculated by
// performing a grid search over several values, and evaluating performance
// metrics for the GUM verified-1000 dataset.

// If all boundary weights of a phrase are less than this value, we
// don't break it further, even if the number of words/characters
// exceeds the threshold.
const float kMinThresholdForBreaking = 1;

// Conversely, if any of the boundary weights of a phrase are more than
// this value, we break it further, even if the number of
// words/characters already meets the threshold.
const float kMaxThresholdForBreaking = 5;

// Returns a vector that has a ramp up and a ramp down for the given length.
static std::vector<float> Triangle(int length) {
  std::vector<float> triangle(length);
  int index = 0;
  for (int i = 0; i < length / 2; i++) {
    triangle[index++] = static_cast<float>(i) / length;
  }
  for (int i = length - length / 2 - 1; i >= 0; i--) {
    triangle[index++] = static_cast<float>(i) / length;
  }
  return triangle;
}

// Checks whether a candidate phrase meets the phrase qualification criterion.
// Given a sentence fragment (indicated by start and end indices in a tokenized
// sentence), and a qualification strategy (i.e. a max number of words or
// characters in the phrase), this function returns whether the phrase
// qualifies.
static bool QualifiesAsPhrase(const TokenizedSentence& tokenized_sentence,
                              int start,
                              int end,
                              Strategy strategy,
                              int threshold) {
  bool qualifies_as_phrase = false;
  int n_words = tokenized_sentence.WordsBetween(start, end);
  int n_characters = tokenized_sentence.CharactersBetween(start, end);
  switch (strategy) {
    case Strategy::kWords:
      qualifies_as_phrase = n_words <= threshold;
      break;
    case Strategy::kCharacters:
      qualifies_as_phrase = (n_words == 1) || (n_characters <= threshold);
      break;
  }
  return qualifies_as_phrase;
}

std::vector<float> PhraseSegmenter::CalculatePhraseBoundariesVector(
    const TokenizedSentence& tokenized_sentence,
    const TokenBoundaries& token_boundaries,
    const Strategy strategy,
    const int threshold) {
  std::vector<float> break_likelihood(
      token_boundaries.boundary_weights().size());

  // Initialize candidates to a single phrase with the entire sentence.
  // The tuple consists of start, end, and recursion depth.
  std::deque<std::tuple<int, int, int>> candidate_phrases;
  candidate_phrases.emplace_back(0, token_boundaries.boundary_weights().size(),
                                 1);

  // This is a modified BFS algorithm, that iteratively breaks longer phrases
  // until all phrases meet the qualification criteria defined by Strategy and
  // threshold.
  while (!candidate_phrases.empty()) {
    auto [start, end, depth] = candidate_phrases.front();
    candidate_phrases.pop_front();

    if (start == end) {
      // Cannot split 1-word phrases
      continue;
    }

    // Calculate weights in the range, and the maximum weight.
    std::vector<float> weights = std::vector<float>(end - start);
    unsigned int index_max = 0;
    for (int i = start + 1; i < end; i++) {
      int index = i - start;
      weights[index] = token_boundaries.boundary_weights()[i];
      if (weights[index] > weights[index_max]) {
        index_max = index;
      }
    }

    // Check if the phrase meets the condition set by strategy and thresholds.

    // Even if the phrase doesn't meet the qualification criteria, if the
    // weights are all low (so no obvious break point), don't break it further.
    // Note that we do this only for words-based breaking, based on an
    // assumption that character-based breaking might be stricter with the
    // threshold (for example, character-based breaking might be used to fit a
    // phrase within a limited amount of screen space).
    if ((weights[index_max] <= kMinThresholdForBreaking) &&
        (strategy == Strategy::kWords)) {
      continue;
    }

    // If the phrase meets the qualification criterion, don't break it further.
    // The exception is if there's an obvious breaking point (as indicated by a
    // really high weight), in which case a further break would be warranted; so
    // we also check if the maximum weight is less than the breaking threshold.
    if (QualifiesAsPhrase(tokenized_sentence, start, end, strategy,
                          threshold) &&
        (weights[index_max] < kMaxThresholdForBreaking)) {
      continue;
    }

    // Add a triangular bias to the weights to bias towards breaking in the
    // center (i.e. generating longer phrases) in the case of ties.
    auto tri = Triangle(end - start);
    for (int i = start; i < end; i++) {
      weights[i - start] += tri[i - start];
    }

    // Insert a phrase break at the boundary with the greatest weight.
    // We need to do this again because we added the triangle.
    index_max = 0;
    for (unsigned int i = 1; i < weights.size(); i++) {
      if (weights[i] > weights[index_max]) {
        index_max = i;
      }
    }

    int break_location = start + index_max;
    // Set the probability to 1.0. This was found to have the best
    // performance for the GUM verified-1000 dataset (English only). Other
    // options that were tried included setting the break likelihood by scaling
    // down the weight, followed by a dynamic programming search for the most
    // likely phrases. These approaches might work better for other languages.
    break_likelihood[break_location] = 1.0f;

    candidate_phrases.emplace_back(start, start + index_max, depth + 1);
    if (index_max < token_boundaries.boundary_weights().size() - 1) {
      candidate_phrases.emplace_back(start + index_max + 1, end, depth + 1);
    }
  }

  return break_likelihood;
}
