// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/content/renderer/language_detection_agent.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/language_detection/content/renderer/language_detection_model_manager.h"
#include "components/language_detection/core/features.h"
#include "components/language_detection/core/language_detection_model.h"
#include "components/language_detection/core/language_detection_provider.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

namespace language_detection {

LanguageDetectionAgent::LanguageDetectionAgent(
    content::RenderFrame* render_frame,
    language_detection::LanguageDetectionModel& language_detection_model)
    : content::RenderFrameObserver(render_frame),
      waiting_for_first_foreground_(render_frame->IsHidden()),
      language_detection_model_(language_detection_model),
      language_detection_model_manager_(language_detection_model) {
  // If the language detection model is available, we do not
  // worry about requesting the model.
  if (language_detection_model_->IsAvailable()) {
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("LanguageDetection.TFLiteModel.WasModelRequestDeferred",
                        waiting_for_first_foreground_);

  // Ensure the render frame is visible, otherwise the browser-side
  // driver may not exist yet (https://crbug.com/1199397).
  if (!waiting_for_first_foreground_) {
    RequestModel();
  }
}

LanguageDetectionAgent::~LanguageDetectionAgent() = default;

void LanguageDetectionAgent::WasShown() {
  // Check if the the render frame was initially hidden and
  // the model request was delayed until the frame was in
  // the foreground.
  if (!waiting_for_first_foreground_) {
    return;
  }

  waiting_for_first_foreground_ = false;

  if (language_detection_model_->IsAvailable()) {
    return;
  }
  // The model request was deferred because the frame was hidden
  // and now the model is visible and the model is still not available.
  // The browser-side translate driver should always be available at
  // this point so we should make the request and race to get the
  // model loaded for when the page content is available.
  RequestModel();
}

void LanguageDetectionAgent::OnDestruct() {
  delete this;
}

void LanguageDetectionAgent::RequestModel() {
  language_detection_model_manager_.GetLanguageDetectionModel(
      render_frame()->GetBrowserInterfaceBroker(),
      base::BindOnce([](LanguageDetectionModel* model) {}));
}
}  // namespace language_detection
