// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/banners/before_install_prompt_event.h"

#include "components/webapps/browser/banners/app_banner_metrics.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace webapps {

BeforeInstallPromptEvent::BeforeInstallPromptEvent(
    content::WebContents* web_contents,
    OnPromptCallback on_prompt,
    OnReplyCallback on_reply)
    : web_contents_(web_contents),
      on_prompt_(std::move(on_prompt)),
      on_reply_(std::move(on_reply)) {}

BeforeInstallPromptEvent::~BeforeInstallPromptEvent() {
  switch (state_) {
    case State::kPendingPromptCanceled:
      TrackBeforeInstallEvent(
          BEFORE_INSTALL_EVENT_PROMPT_NOT_CALLED_AFTER_PREVENT_DEFAULT);
      break;
    case State::kPendingPromptNotCanceled:
      TrackBeforeInstallEvent(
          BEFORE_INSTALL_EVENT_PROMPT_NOT_CALLED_NOT_CANCELLED);
      break;
    default:
      break;
  }
}

void BeforeInstallPromptEvent::Send(std::vector<std::string> platforms) {
  mojo::Remote<blink::mojom::AppBannerController> controller;
  web_contents_->GetPrimaryMainFrame()->GetRemoteInterfaces()->GetInterface(
      controller.BindNewPipeAndPassReceiver());

  TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_CREATED);

  blink::mojom::AppBannerController* controller_ptr = controller.get();
  controller_ptr->BannerPromptRequest(
      receiver_.BindNewPipeAndPassRemote(), event_.BindNewPipeAndPassReceiver(),
      platforms,
      base::BindOnce(&BeforeInstallPromptEvent::OnEventReply,
                     weak_factory_.GetWeakPtr(), std::move(controller)));
}

bool BeforeInstallPromptEvent::IsPromptAvailable() const {
  return receiver_.is_bound();
}

void BeforeInstallPromptEvent::SendBannerAccepted(const std::string& platform) {
  if (event_.is_bound()) {
    event_->BannerAccepted(platform);
    event_.reset();
  }
}

void BeforeInstallPromptEvent::SendBannerDismissed() {
  if (event_.is_bound()) {
    event_->BannerDismissed();
    event_.reset();
  }
}

bool BeforeInstallPromptEvent::IsPendingPrompt() const {
  return state_ == State::kPendingPromptCanceled ||
         state_ == State::kPendingPromptNotCanceled;
}

bool BeforeInstallPromptEvent::IsRunning() const {
  return state_ == State::kSending || state_ == State::kSendingGotEarlyPrompt;
}

void BeforeInstallPromptEvent::DisplayAppBanner() {
  // Prevent this from being called multiple times on the same connection.
  receiver_.reset();

  if (state_ == State::kPendingPromptCanceled) {
    TrackBeforeInstallEvent(
        BEFORE_INSTALL_EVENT_PROMPT_CALLED_AFTER_PREVENT_DEFAULT);
    state_ = State::kComplete;
    std::move(on_prompt_).Run();
  } else if (state_ == State::kPendingPromptNotCanceled) {
    TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_PROMPT_CALLED_NOT_CANCELED);
    state_ = State::kComplete;
    std::move(on_prompt_).Run();
  } else if (state_ == State::kSending) {
    TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_EARLY_PROMPT);
    state_ = State::kSendingGotEarlyPrompt;
  }
}

void BeforeInstallPromptEvent::OnEventReply(
    mojo::Remote<blink::mojom::AppBannerController> controller,
    blink::mojom::AppBannerPromptReply reply) {
  bool event_canceled = reply == blink::mojom::AppBannerPromptReply::CANCEL;
  if (event_canceled) {
    TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_PREVENT_DEFAULT_CALLED);
    web_contents_->GetPrimaryMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kInfo,
        "Banner not shown: beforeinstallpromptevent.preventDefault() called. "
        "The page must call beforeinstallpromptevent.prompt() to show the "
        "banner.");
  }

  if (state_ == State::kSending) {
    if (!event_canceled) {
      state_ = State::kPendingPromptNotCanceled;
    } else {
      state_ = State::kPendingPromptCanceled;
    }
    std::move(on_reply_).Run(event_canceled);
    return;
  }

  DCHECK_EQ(State::kSendingGotEarlyPrompt, state_);
  state_ = State::kComplete;
  std::move(on_prompt_).Run();
}

}  // namespace webapps
