// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CONTENT_RENDERER_LANGUAGE_DETECTION_AGENT_H_
#define COMPONENTS_LANGUAGE_DETECTION_CONTENT_RENDERER_LANGUAGE_DETECTION_AGENT_H_

#include "base/memory/weak_ptr.h"
#include "components/language_detection/content/renderer/language_detection_model_manager.h"
#include "content/public/renderer/render_frame_observer.h"

namespace language_detection {

// This class deals with language detection for translate.
// There is one LanguageDetectionAgent per RenderView.
// TODO(https://crbug.com/380171876): Move this back into
// components/translate.
class LanguageDetectionAgent : public content::RenderFrameObserver {
 public:
  explicit LanguageDetectionAgent(
      content::RenderFrame* render_frame,
      language_detection::LanguageDetectionModel& language_detection_model);

  LanguageDetectionAgent(const LanguageDetectionAgent&) = delete;
  LanguageDetectionAgent& operator=(const LanguageDetectionAgent&) = delete;

  ~LanguageDetectionAgent() override;

  bool waiting_for_first_foreground() { return waiting_for_first_foreground_; }

 private:
  // content::RenderFrameObserver implementation.
  void WasShown() override;
  void OnDestruct() override;

  void RequestModel();

  // Whether the render frame observed by |this| was initially hidden and
  // the request for a model is delayed until the frame is in the foreground.
  bool waiting_for_first_foreground_;

  // Not owned by `this`. It must outlive `this`.
  const raw_ref<language_detection::LanguageDetectionModel>
      language_detection_model_;

  LanguageDetectionModelManager language_detection_model_manager_;

  // Weak pointer factory used to provide references to the translate host.
  base::WeakPtrFactory<LanguageDetectionAgent> weak_pointer_factory_{this};
};

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CONTENT_RENDERER_LANGUAGE_DETECTION_AGENT_H_
