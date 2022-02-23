// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_input_method_observer.h"

#include "base/logging.h"
#include "chromecast/browser/webview/proto/webview.pb.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"

namespace chromecast {
namespace {

webview::TextInputType ConvertTextInputType(
    const ui::TextInputType text_input_type) {
  switch (text_input_type) {
    case ui::TEXT_INPUT_TYPE_NONE:
      return webview::TEXT_INPUT_TYPE_NONE;
    case ui::TEXT_INPUT_TYPE_TEXT:
      return webview::TEXT_INPUT_TYPE_TEXT;
    case ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE:
      return webview::TEXT_INPUT_TYPE_CONTENT_EDITABLE;
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return webview::TEXT_INPUT_TYPE_PASSWORD;
    case ui::TEXT_INPUT_TYPE_SEARCH:
      return webview::TEXT_INPUT_TYPE_SEARCH;
    case ui::TEXT_INPUT_TYPE_EMAIL:
      return webview::TEXT_INPUT_TYPE_EMAIL;
    case ui::TEXT_INPUT_TYPE_NUMBER:
      return webview::TEXT_INPUT_TYPE_NUMBER;
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
      return webview::TEXT_INPUT_TYPE_TELEPHONE;
    case ui::TEXT_INPUT_TYPE_DATE:
      return webview::TEXT_INPUT_TYPE_DATE;
    case ui::TEXT_INPUT_TYPE_DATE_TIME:
      return webview::TEXT_INPUT_TYPE_DATE_TIME;
    case ui::TEXT_INPUT_TYPE_MONTH:
      return webview::TEXT_INPUT_TYPE_MONTH;
    case ui::TEXT_INPUT_TYPE_TIME:
      return webview::TEXT_INPUT_TYPE_TIME;
    case ui::TEXT_INPUT_TYPE_URL:
      return webview::TEXT_INPUT_TYPE_URL;
    case ui::TEXT_INPUT_TYPE_WEEK:
      return webview::TEXT_INPUT_TYPE_WEEK;
    case ui::TEXT_INPUT_TYPE_TEXT_AREA:
      return webview::TEXT_INPUT_TYPE_TEXT_AREA;
    case ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD:
      return webview::TEXT_INPUT_TYPE_DATE_TIME_FIELD;
    case ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
      return webview::TEXT_INPUT_TYPE_DATE_TIME_LOCAL;
    case ui::TEXT_INPUT_TYPE_NULL:
      return webview::TEXT_INPUT_TYPE_NULL;
  }
  LOG(ERROR) << "Unmapped TextInputType: " << text_input_type;
  return webview::TEXT_INPUT_TYPE_NULL;
}

}  // namespace

void WebviewInputMethodObserver::OnTextInputStateChanged(
    const ui::TextInputClient* text_input_client) {
  if (!text_input_client || !controller_->client())
    return;
  std::unique_ptr<chromecast::webview::WebviewResponse> focus_event_response =
      std::make_unique<chromecast::webview::WebviewResponse>();
  auto* focus_event = focus_event_response->mutable_input_focus_event();
  focus_event->set_flags(text_input_client->GetTextInputFlags());
  focus_event->set_type(
      ConvertTextInputType(text_input_client->GetTextInputType()));
  last_focus_response_ = std::make_unique<chromecast::webview::WebviewResponse>(
      *focus_event_response);
  controller_->client()->EnqueueSend(std::move(focus_event_response));
}

WebviewInputMethodObserver::WebviewInputMethodObserver(
    chromecast::WebContentController* controller,
    ui::InputMethod* input_method)
    : controller_(controller), input_method_(input_method) {
  input_method_->AddObserver(this);
}

WebviewInputMethodObserver::~WebviewInputMethodObserver() {
  if (input_method_) {
    input_method_->RemoveObserver(this);
  }
}

void WebviewInputMethodObserver::OnInputMethodDestroyed(
    const ui::InputMethod* input_method) {
  input_method_ = nullptr;
}

void WebviewInputMethodObserver::OnVirtualKeyboardVisibilityChangedIfEnabled(
    bool should_show) {
  if (should_show) {
    std::unique_ptr<chromecast::webview::WebviewResponse>
        last_focus_response_copy =
            std::make_unique<chromecast::webview::WebviewResponse>(
                *last_focus_response_);
    controller_->client()->EnqueueSend(std::move(last_focus_response_copy));
  }
}

}  // namespace chromecast
