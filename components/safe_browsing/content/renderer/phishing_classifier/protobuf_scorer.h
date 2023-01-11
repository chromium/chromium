// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class loads a client-side protobuf model from a
// string and lets you compute a phishing score
// for a set of previously extracted features.  The phishing score corresponds
// to the probability that the features are indicative of a phishing site.
//
// For more details on how the score is actually computed for a given model
// and a given set of features read the comments in client_model.proto file.
//
// See features.h for a list of features that are currently used.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PROTOBUF_SCORER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PROTOBUF_SCORER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <unordered_set>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/callback.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace safe_browsing {
class FeatureMap;

class ProtobufModelScorer : public Scorer {
 public:
  ~ProtobufModelScorer() override;

  // Factory method which creates a new Scorer object by parsing the given
  // model. If parsing fails this method returns NULL.
  // Can use this if model_str is empty.
  static std::unique_ptr<ProtobufModelScorer> Create(
      const base::StringPiece& model_str,
      base::File visual_tflite_model);

  double ComputeScore(const FeatureMap& features) const override;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  void ApplyVisualTfLiteModel(
      const SkBitmap& bitmap,
      base::OnceCallback<void(std::vector<double>)> callback) const override;
#endif

  int model_version() const override;
  int dom_model_version() const override;
  base::RepeatingCallback<bool(uint32_t)> find_page_word_callback()
      const override;
  base::RepeatingCallback<bool(const std::string&)> find_page_term_callback()
      const override;
  size_t max_words_per_term() const override;
  uint32_t murmurhash3_seed() const override;
  size_t max_shingles_per_page() const override;
  size_t shingle_size() const override;
  float threshold_probability() const override;
  int tflite_model_version() const override;
  const google::protobuf::RepeatedPtrField<TfLiteModelMetadata::Threshold>&
  tflite_thresholds() const override;

  const std::unordered_set<std::string>& get_page_terms_for_test() const;
  const std::unordered_set<uint32_t>& get_page_words_for_test() const;

 private:
  friend class PhishingScorerTest;
  friend class Scorer;

  bool has_page_term(const std::string& str) const;
  bool has_page_word(uint32_t page_word_hash) const;

  ProtobufModelScorer();

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
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PROTOBUF_SCORER_H_
