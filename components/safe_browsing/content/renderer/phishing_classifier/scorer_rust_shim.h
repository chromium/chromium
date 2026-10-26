// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_SCORER_RUST_SHIM_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_SCORER_RUST_SHIM_H_

#include <stdint.h>

#include <memory>

#include "base/files/file.h"

// C++ functions that allow the Rust PhishingModelSetter implementation to
// create, update, or clear the process-wide phishing Scorer in ScorerStorage.

namespace safe_browsing::rust_bridge {

// Builds a Scorer from `classification_model` and installs it in ScorerStorage.
// Returns true if a Scorer was created and installed.
bool SetPhishingTfLiteModel(int32_t classification_input_width,
                            int32_t classification_input_height,
                            std::unique_ptr<base::File> classification_model);

// Builds a Scorer with both a classification model and an image-embedding model
// and installs it in ScorerStorage. Returns true if a Scorer was created and
// installed.
bool SetImageEmbeddingAndPhishingTfLiteModel(
    int32_t classification_input_width,
    int32_t classification_input_height,
    std::unique_ptr<base::File> classification_model,
    int32_t image_embedding_input_width,
    int32_t image_embedding_input_height,
    std::unique_ptr<base::File> image_embedding_model);

// Attaches an image-embedding model to the Scorer currently in ScorerStorage.
// Does nothing if no Scorer is installed.
void AttachImageEmbeddingModelAndDimensions(
    int32_t image_embedding_input_width,
    int32_t image_embedding_input_height,
    std::unique_ptr<base::File> image_embedding_model);

// Clears the active Scorer in ScorerStorage.
void ClearScorer();

}  // namespace safe_browsing::rust_bridge

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_SCORER_RUST_SHIM_H_
