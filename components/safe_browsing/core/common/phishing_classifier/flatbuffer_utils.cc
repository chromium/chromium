// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/phishing_classifier/flatbuffer_utils.h"

namespace safe_browsing {

bool VerifyCSDFlatBufferIndicesAndFields(const flat::ClientSideModel* model) {
  if (!model) {
    return false;
  }

  const flat::TfLiteModelMetadata* metadata = model->tflite_metadata();
  if (!metadata) {
    return false;
  }

  const flatbuffers::Vector<flatbuffers::Offset<flat::Hash>>* hashes =
      model->hashes();
  if (!hashes) {
    return false;
  }

  const flatbuffers::Vector<flatbuffers::Offset<flat::ClientSideModel_::Rule>>*
      rules = model->rule();
  if (!rules) {
    return false;
  }
  for (const flat::ClientSideModel_::Rule* rule : *rules) {
    if (!rule || !rule->feature()) {
      return false;
    }
    for (int32_t feature : *rule->feature()) {
      if (feature < 0 || feature >= static_cast<int32_t>(hashes->size())) {
        return false;
      }
    }
  }

  const flatbuffers::Vector<int32_t>* page_terms = model->page_term();
  if (!page_terms) {
    return false;
  }
  for (int32_t page_term_idx : *page_terms) {
    if (page_term_idx < 0 ||
        page_term_idx >= static_cast<int32_t>(hashes->size())) {
      return false;
    }
  }

  const flatbuffers::Vector<uint32_t>* page_words = model->page_word();
  if (!page_words) {
    return false;
  }

  const flatbuffers::Vector<
      flatbuffers::Offset<flat::TfLiteModelMetadata_::Threshold>>* thresholds =
      metadata->thresholds();
  if (!thresholds) {
    return false;
  }
  for (const flat::TfLiteModelMetadata_::Threshold* threshold : *thresholds) {
    if (!threshold || !threshold->label()) {
      return false;
    }
  }

  return true;
}

}  // namespace safe_browsing
