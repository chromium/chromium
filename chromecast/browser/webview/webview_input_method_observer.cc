// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_input_method_observer.h"

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
      break;
    case ui::TEXT_INPUT_TYPE_TEXT:
      return webview::TEXT_INPUT_TYPE_TEXT;
      break;
    case ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE:
      return webview::TEXT_INPUT_TYPE_CONTENT_EDITABLE;
      break;
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return webview::TEXT_INPUT_TYPE_PASSWORD;
      break;
    case ui::TEXT_INPUT_TYPE_SEARCH:
      return webview::TEXT_INPUT_TYPE_SEARCH;
      break;
    case ui::TEXT_INPUT_TYPE_EMAIL:
      return webview::TEXT_INPUT_TYPE_EMAIL;
      break;
    case ui::TEXT_INPUT_TYPE_NUMBER:
      return webview::TEXT_INPUT_TYPE_NUMBER;
      break;
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
      return webview::TEXT_INPUT_TYPE_TELEPHONE;
      break;
    case ui::TEXT_INPUT_TYPE_DATE:
      return webview::TEXT_INPUT_TYPE_DATE;
      break;
    case ui::TEXT_INPUT_TYPE_DATE_TIME:
      return webview::TEXT_INPUT_TYPE_DATE_TIME;
      break;
    case ui::TEXT_INPUT_TYPE_MONTH:
      return webview::TEXT_INPUT_TYPE_MONTH;
      break;
    case ui::TEXT_INPUT_TYPE_TIME:
      return webview::TEXT_INPUT_TYPE_TIME;
      break;
    case ui::TEXT_INPUT_TYPE_URL:
      return webview::TEXT_INPUT_TYPE_URL;
      break;
    case ui::TEXT_INPUT_TYPE_WEEK:
      return webview::TEXT_INPUT_TYPE_WEEK;
      break;
    case ui::TEXT_INPUT_TYPE_TEXT_AREA:
      return webview::TEXT_INPUT_TYPE_TEXT_AREA;
      break;
    case ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD:
      return webview::TEXT_INPUT_TYPE_DATE_TIME_FIELD;
      break;
    case ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
      return webview::TEXT_INPUT_TYPE_DATE_TIME_LOCAL;
      break;
    case ui::TEXT_INPUT_TYPE_NULL:
      return webview::TEXT_INPUT_TYPE_NULL;
      break;
  }
  LOG(ERROR) << "Unmapped TextInputType: " << text_input_type;
  return webview::TEXT_INPUT_TYPE_NULL;
}

}  // namespace

void WebviewInputMethodObserver::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  if (!client)
    return;
  std::unique_ptr<chromecast::webview::WebviewResponse> focus_event_response =
      std::make_unique<chromecast::webview::WebviewResponse>();
  auto* focus_event = focus_event_response->mutable_input_focus_event();
  focus_event->set_flags(client->GetTextInputFlags());
  focus_event->set_type(ConvertTextInputType(client->GetTextInputType()));
  client_->EnqueueSend(std::move(focus_event_response));
}

WebviewInputMethodObserver::WebviewInputMethodObserver(
    chromecast::WebContentController::Client* client,
    ui::InputMethod* input_method)
    : client_(client), input_method_(input_method) {
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

}  // namespace chromecast
