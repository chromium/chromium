// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CONTENT_RENDERER_LANGUAGE_DETECTION_AGENT_H_
#define COMPONENTS_LANGUAGE_DETECTION_CONTENT_RENDERER_LANGUAGE_DETECTION_AGENT_H_

#include "base/memory/weak_ptr.h"
#include "components/language_detection/content/common/language_detection.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace language_detection {

// This class deals with language detection.
// There is one LanguageDetectionAgent per RenderView.
class LanguageDetectionAgent : public content::RenderFrameObserver {
 public:
  explicit LanguageDetectionAgent(content::RenderFrame* render_frame);

  LanguageDetectionAgent(const LanguageDetectionAgent&) = delete;
  LanguageDetectionAgent& operator=(const LanguageDetectionAgent&) = delete;

  ~LanguageDetectionAgent() override;

  const mojo::Remote<mojom::ContentLanguageDetectionDriver>&
  GetLanguageDetectionHandler();

  // Called by the translate host when a new language detection model file
  // has been loaded and is available.
  void UpdateLanguageDetectionModel(base::File model_file);

  bool waiting_for_first_foreground() { return waiting_for_first_foreground_; }

 private:
  // content::RenderFrameObserver implementation.
  void WasShown() override;
  void OnDestruct() override;

  // Whether the render frame observed by |this| was initially hidden and
  // the request for a model is delayed until the frame is in the foreground.
  bool waiting_for_first_foreground_;

  mojo::Remote<mojom::ContentLanguageDetectionDriver>
      language_detection_handler_;

  // Weak pointer factory used to provide references to the translate host.
  base::WeakPtrFactory<LanguageDetectionAgent> weak_pointer_factory_{this};
};

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CONTENT_RENDERER_LANGUAGE_DETECTION_AGENT_H_
