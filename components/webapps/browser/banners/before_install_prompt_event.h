// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_BANNERS_BEFORE_INSTALL_PROMPT_EVENT_H_
#define COMPONENTS_WEBAPPS_BROWSER_BANNERS_BEFORE_INSTALL_PROMPT_EVENT_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/app_banner/app_banner.mojom.h"

namespace content {
class WebContents;
}

namespace webapps {

class BeforeInstallPromptEvent : public blink::mojom::AppBannerService {
 public:
  // Called when the page calls prompt() on the event.
  using OnPromptCallback = base::OnceClosure;

  // Called when the event reply is received.
  // |event_canceled| is true if preventDefault() was called on the event.
  using OnReplyCallback = base::OnceCallback<void(bool event_canceled)>;

  BeforeInstallPromptEvent(content::WebContents* web_contents,
                           OnPromptCallback on_prompt,
                           OnReplyCallback on_reply);
  ~BeforeInstallPromptEvent() override;

  BeforeInstallPromptEvent(const BeforeInstallPromptEvent&) = delete;
  BeforeInstallPromptEvent& operator=(const BeforeInstallPromptEvent&) = delete;

  // Sends the beforeinstallprompt event to the page.
  void Send(std::vector<std::string> platforms);

  // Returns true if the page can call prompt() (i.e. receiver is still bound).
  bool IsPromptAvailable() const;

  // Sends banner accepted/dismissed to the page if the Mojo event is bound.
  void SendBannerAccepted(const std::string& platform);
  void SendBannerDismissed();

  // Returns true if the event has been sent and we are waiting for the page to
  // call prompt().
  bool IsPendingPrompt() const;

  // Returns true if the event is still being sent (waiting for reply) and has
  // not yet been prompted.
  bool IsRunning() const;

  // blink::mojom::AppBannerService:
  void DisplayAppBanner() override;

 private:
  void OnEventReply(mojo::Remote<blink::mojom::AppBannerController> controller,
                    blink::mojom::AppBannerPromptReply reply);

  enum class State {
    kSending,
    kSendingGotEarlyPrompt,
    kPendingPromptNotCanceled,
    kPendingPromptCanceled,
    kComplete,
  };

  raw_ptr<content::WebContents> web_contents_;
  OnPromptCallback on_prompt_;
  OnReplyCallback on_reply_;

  State state_ = State::kSending;

  mojo::Receiver<blink::mojom::AppBannerService> receiver_{this};
  mojo::Remote<blink::mojom::AppBannerEvent> event_;

  base::WeakPtrFactory<BeforeInstallPromptEvent> weak_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_BANNERS_BEFORE_INSTALL_PROMPT_EVENT_H_
