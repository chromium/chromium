// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/scorer_rust_shim.h"

#include <utility>

#include "base/files/file.h"
#include "components/safe_browsing/core/common/phishing_classifier/scorer.h"

namespace safe_browsing::rust_bridge {

namespace {

base::File UnboxFile(std::unique_ptr<base::File> file) {
  return file ? std::move(*file) : base::File();
}

}  // namespace

bool SetPhishingTfLiteModel(int32_t classification_input_width,
                            int32_t classification_input_height,
                            std::unique_ptr<base::File> classification_model) {
  std::unique_ptr<Scorer> scorer =
      Scorer::Create(classification_input_width, classification_input_height,
                     UnboxFile(std::move(classification_model)));
  if (!scorer) {
    return false;
  }
  ScorerStorage::GetInstance()->SetScorer(std::move(scorer));
  return true;
}

bool SetImageEmbeddingAndPhishingTfLiteModel(
    int32_t classification_input_width,
    int32_t classification_input_height,
    std::unique_ptr<base::File> classification_model,
    int32_t image_embedding_input_width,
    int32_t image_embedding_input_height,
    std::unique_ptr<base::File> image_embedding_model) {
  std::unique_ptr<Scorer> scorer = Scorer::CreateScorerWithImageEmbeddingModel(
      classification_input_width, classification_input_height,
      UnboxFile(std::move(classification_model)), image_embedding_input_width,
      image_embedding_input_height,
      UnboxFile(std::move(image_embedding_model)));
  if (!scorer) {
    return false;
  }
  ScorerStorage::GetInstance()->SetScorer(std::move(scorer));
  return true;
}

void AttachImageEmbeddingModelAndDimensions(
    int32_t image_embedding_input_width,
    int32_t image_embedding_input_height,
    std::unique_ptr<base::File> image_embedding_model) {
  Scorer* scorer = ScorerStorage::GetInstance()->GetScorer();
  if (!scorer) {
    return;
  }
  scorer->AttachImageEmbeddingModel(
      image_embedding_input_width, image_embedding_input_height,
      UnboxFile(std::move(image_embedding_model)));
}

void ClearScorer() {
  ScorerStorage::GetInstance()->ClearScorer();
}

}  // namespace safe_browsing::rust_bridge
