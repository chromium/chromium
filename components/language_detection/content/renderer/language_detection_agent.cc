// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/content/renderer/language_detection_agent.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "components/language_detection/core/language_detection_model.h"
#include "components/language_detection/core/language_detection_provider.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

namespace language_detection {

LanguageDetectionAgent::LanguageDetectionAgent(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      waiting_for_first_foreground_(render_frame->IsHidden()) {
  language_detection::LanguageDetectionModel& language_detection_model =
      language_detection::GetLanguageDetectionModel();

  // If the language detection model is available, we do not
  // worry about requesting the model.
  if (language_detection_model.IsAvailable()) {
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("LanguageDetection.TFLiteModel.WasModelRequestDeferred",
                        waiting_for_first_foreground_);

  // Ensure the render frame is visible, otherwise the browser-side
  // driver may not exist yet (https://crbug.com/1199397).
  if (!waiting_for_first_foreground_) {
    GetLanguageDetectionHandler()->GetLanguageDetectionModel(
        base::BindOnce(&LanguageDetectionAgent::UpdateLanguageDetectionModel,
                       weak_pointer_factory_.GetWeakPtr()));
  }
}

LanguageDetectionAgent::~LanguageDetectionAgent() = default;

const mojo::Remote<mojom::ContentLanguageDetectionDriver>&
LanguageDetectionAgent::GetLanguageDetectionHandler() {
  if (language_detection_handler_) {
    if (language_detection_handler_.is_connected()) {
      return language_detection_handler_;
    }
    // The handler can become unbound or disconnected in testing so this catches
    // that case and reconnects so `this` can connect to the driver in the
    // browser.
    language_detection_handler_.reset();
  }

  render_frame()->GetBrowserInterfaceBroker().GetInterface(
      language_detection_handler_.BindNewPipeAndPassReceiver());
  return language_detection_handler_;
}

void LanguageDetectionAgent::UpdateLanguageDetectionModel(
    base::File model_file) {
  TRACE_EVENT("browser", "TranslateAgent::UpdateLanguageDetectionModel");
  base::ScopedUmaHistogramTimer timer(
      "LanguageDetection.TFLiteModel.UpdateLanaguageDetectionModelTime");
  language_detection::LanguageDetectionModel& language_detection_model =
      language_detection::GetLanguageDetectionModel();
  language_detection_model.UpdateWithFile(std::move(model_file));
}

void LanguageDetectionAgent::WasShown() {
  // Check if the the render frame was initially hidden and
  // the model request was delayed until the frame was in
  // the foreground.
  if (!waiting_for_first_foreground_) {
    return;
  }

  waiting_for_first_foreground_ = false;

  language_detection::LanguageDetectionModel& language_detection_model =
      language_detection::GetLanguageDetectionModel();
  if (language_detection_model.IsAvailable()) {
    return;
  }
  // The model request was deferred because the frame was hidden
  // and now the model is visible and the model is still not available.
  // The browser-side translate driver should always be available at
  // this point so we should make the request and race to get the
  // model loaded for when the page content is available.
  GetLanguageDetectionHandler()->GetLanguageDetectionModel(
      base::BindOnce(&LanguageDetectionAgent::UpdateLanguageDetectionModel,
                     weak_pointer_factory_.GetWeakPtr()));
}

void LanguageDetectionAgent::OnDestruct() {
  delete this;
}

}  // namespace language_detection
