// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class loads a client-side model and lets you compute a phishing score
// for a set of previously extracted features.  The phishing score corresponds
// to the probability that the features are indicative of a phishing site.
//
// For more details on how the score is actually computed for a given model
// and a given set of features read the comments in client_model.proto file.
//
// See features.h for a list of features that are currently used.

#ifndef CHROME_RENDERER_SAFE_BROWSING_SCORER_H_
#define CHROME_RENDERER_SAFE_BROWSING_SCORER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <unordered_set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "components/safe_browsing/core/proto/client_model.pb.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace safe_browsing {
class FeatureMap;

// Scorer methods are virtual to simplify mocking of this class.
class Scorer {
 public:
  virtual ~Scorer();

  // Factory method which creates a new Scorer object by parsing the given
  // model.  If parsing fails this method returns NULL.
  static Scorer* Create(const base::StringPiece& model_str);

  // This method computes the probability that the given features are indicative
  // of phishing.  It returns a score value that falls in the range [0.0,1.0]
  // (range is inclusive on both ends).
  virtual double ComputeScore(const FeatureMap& features) const;

  // This method matches the given |bitmap| against the visual model. It
  // modifies |request| appropriately, and returns the new request. This expects
  // to be called on the renderer main thread, but will perform scoring
  // asynchronously on a worker thread.
  virtual void GetMatchingVisualTargets(
      const SkBitmap& bitmap,
      std::unique_ptr<ClientPhishingRequest> request,
      base::OnceCallback<void(std::unique_ptr<ClientPhishingRequest>)> callback)
      const;

  // Returns the version number of the loaded client model.
  int model_version() const;

  // -- Accessors used by the page feature extractor ---------------------------

  // Returns a set of hashed page terms that appear in the model in binary
  // format.
  const std::unordered_set<std::string>& page_terms() const;

  // Returns a set of hashed page words that appear in the model in binary
  // format.
  const std::unordered_set<uint32_t>& page_words() const;

  // Return the maximum number of words per term for the loaded model.
  size_t max_words_per_term() const;

  // Returns the murmurhash3 seed for the loaded model.
  uint32_t murmurhash3_seed() const;

  // Return the maximum number of unique shingle hashes per page.
  size_t max_shingles_per_page() const;

  // Return the number of words in a shingle.
  size_t shingle_size() const;

  // Returns the threshold probability above which we send a CSD ping.
  float threshold_probability() const;

 protected:
  // Most clients should use the factory method.  This constructor is public
  // to allow for mock implementations.
  Scorer();

 private:
  friend class PhishingScorerTest;

  // Computes the score for a given rule and feature map.  The score is computed
  // by multiplying the rule weight with the product of feature weights for the
  // given rule.  The feature weights are stored in the feature map.  If a
  // particular feature does not exist in the feature map we set its weight to
  // zero.
  double ComputeRuleScore(const ClientSideModel::Rule& rule,
                          const FeatureMap& features) const;

  ClientSideModel model_;
  std::unordered_set<std::string> page_terms_;
  std::unordered_set<uint32_t> page_words_;

  base::WeakPtrFactory<Scorer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Scorer);
};
}  // namespace safe_browsing

#endif  // CHROME_RENDERER_SAFE_BROWSING_SCORER_H_
