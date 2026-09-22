// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_HANDLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/webui/default_browser/default_browser_modal.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebUI;
}

// The handler for Javascript messages related to the "default-browser-modal"
// page.
class DefaultBrowserModalHandler final
    : public default_browser_modal::mojom::PageHandler {
 public:
  DefaultBrowserModalHandler(
      content::WebUI* web_ui,
      mojo::PendingRemote<default_browser_modal::mojom::Page> page,
      mojo::PendingReceiver<default_browser_modal::mojom::PageHandler>
          receiver);

  DefaultBrowserModalHandler(const DefaultBrowserModalHandler&) = delete;
  DefaultBrowserModalHandler& operator=(const DefaultBrowserModalHandler&) =
      delete;

  ~DefaultBrowserModalHandler() override;

  // default_browser_modal::mojom::PageHandler:
  void Cancel() override;
  void Confirm() override;
  void TryAgain() override;
  void ShowUI() override;
  void CheckDefaultStatusAndMaybeClose(
      CheckDefaultStatusAndMaybeCloseCallback callback) override;

 private:
  void OnHasAcceptedChanged(bool has_accepted);

  void OnCheckDefaultStatusResult(
      CheckDefaultStatusAndMaybeCloseCallback callback,
      shell_integration::DefaultWebClientState state);

  raw_ptr<content::WebUI> web_ui_;
  mojo::Remote<default_browser_modal::mojom::Page> page_;
  mojo::Receiver<default_browser_modal::mojom::PageHandler> receiver_;
  base::CallbackListSubscription has_accepted_subscription_;
  base::WeakPtrFactory<DefaultBrowserModalHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_HANDLER_H_
