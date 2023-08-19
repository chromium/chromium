// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PhishingVisualFeatureExtractor handles capturing images from the renderer
// frame. Once the renderer frame is captured and stored into cc:PaintRecord, a
// SkBitmap will be created, which will then be used for producing the image
// embedding feature vector.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_VISUAL_FEATURE_EXTRACTOR_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_VISUAL_FEATURE_EXTRACTOR_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "cc/paint/paint_recorder.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace safe_browsing {

class PhishingVisualFeatureExtractor {
 public:
  using DoneCallback = base::OnceCallback<void(std::unique_ptr<SkBitmap>)>;

  PhishingVisualFeatureExtractor();

  PhishingVisualFeatureExtractor(const PhishingVisualFeatureExtractor&) =
      delete;
  PhishingVisualFeatureExtractor& operator=(
      const PhishingVisualFeatureExtractor&) = delete;

  ~PhishingVisualFeatureExtractor();

  void ExtractFeatures(blink::WebLocalFrame* frame, DoneCallback done_callback);

 private:
  void CancelPendingExtraction();

  // Runs |done_callback_| and then clears all internal state.
  void RunCallback(std::unique_ptr<SkBitmap> bitmap);

  DoneCallback done_callback_;

  base::WeakPtrFactory<PhishingVisualFeatureExtractor> weak_factory_{this};
};
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_PHISHING_CLASSIFIER_PHISHING_VISUAL_FEATURE_EXTRACTOR_H_
