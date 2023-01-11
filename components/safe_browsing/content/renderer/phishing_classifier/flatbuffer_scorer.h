// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class loads a client-side flatbuffer model from a
// ReadOnlySharedMemoryRegion and lets you compute a phishing score
// for a set of previously extracted features.  The phishing score corresponds
// to the probability that the features are indicative of a phishing site.
//
// For more details on how the score is actually computed for a given model
// and a given set of features read the comments in client_model.fbs file.
//
// See features.h for a list of features that are currently used.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_FLATBUFFER_SCORER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_FLATBUFFER_SCORER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <unordered_set>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace safe_browsing {
class FeatureMap;

class FlatBufferModelScorer : public Scorer {
 public:
  ~FlatBufferModelScorer() override;

  // Factory method which creates a new Scorer object by parsing the given
  // flatbuffer or tflite model. If parsing fails this method returns NULL.
  // Use this only if region is valid.
  static std::unique_ptr<FlatBufferModelScorer> Create(
      base::ReadOnlySharedMemoryRegion region,
      base::File visual_tflite_model);

  double ComputeScore(const FeatureMap& features) const override;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  void ApplyVisualTfLiteModel(
      const SkBitmap& bitmap,
      base::OnceCallback<void(std::vector<double>)> callback) const override;
#endif

  int model_version() const override;
  int dom_model_version() const override;
  size_t max_words_per_term() const override;
  uint32_t murmurhash3_seed() const override;
  size_t max_shingles_per_page() const override;
  size_t shingle_size() const override;
  float threshold_probability() const override;
  int tflite_model_version() const override;
  const google::protobuf::RepeatedPtrField<TfLiteModelMetadata::Threshold>&
  tflite_thresholds() const override;
  base::RepeatingCallback<bool(uint32_t)> find_page_word_callback()
      const override;
  base::RepeatingCallback<bool(const std::string&)> find_page_term_callback()
      const override;

 private:
  friend class PhishingScorerTest;

  bool has_page_term(const std::string& str) const;
  bool has_page_word(uint32_t page_word_hash) const;
  FlatBufferModelScorer();

  double ComputeRuleScore(const flat::ClientSideModel_::Rule* rule,
                          const FeatureMap& features) const;

  // Unowned. Points within flatbuffer_mapping_ and should not be free()d.
  // It remains valid till flatbuffer_mapping_ is valid and should be reassigned
  // if the mapping is updated.
  const flat::ClientSideModel* flatbuffer_model_;
  base::ReadOnlySharedMemoryMapping flatbuffer_mapping_;
  google::protobuf::RepeatedPtrField<TfLiteModelMetadata::Threshold>
      thresholds_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_FLATBUFFER_SCORER_H_
