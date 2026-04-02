// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_AIM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_AIM_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/webui/omnibox_popup/mojom/omnibox_popup_aim.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class OmniboxAimPopupWebUIContent;

namespace content {
class WebContents;
}

namespace gfx {
class Point;
}

class OmniboxPopupAimHandler : public omnibox_popup_aim::mojom::PageHandler {
 public:
  OmniboxPopupAimHandler(
      mojo::PendingReceiver<omnibox_popup_aim::mojom::PageHandler> receiver,
      mojo::PendingRemote<omnibox_popup_aim::mojom::Page> page,
      content::WebContents* web_contents);

  OmniboxPopupAimHandler(const OmniboxPopupAimHandler&) = delete;
  OmniboxPopupAimHandler& operator=(const OmniboxPopupAimHandler&) = delete;

  ~OmniboxPopupAimHandler() override;

  void set_embedder(
      base::WeakPtr<TopChromeWebUIController::Embedder> embedder) {
    embedder_ = embedder;
  }

  // omnibox_popup_aim::mojom::PageHandler:
  // Forwards a close event from the page to the browser.
  void RequestClose() override;
  void ShowContextMenu(const gfx::Point& point) override;

  // Forwards an `OnPopupShown()` call to the page.
  void OnPopupShown(std::unique_ptr<SearchboxContextData::Context> context);

  // Sets whether the context should be preserved when the popup is closed. This
  // value is reset to false when the popup is shown again.
  void SetPreserveContextOnClose(bool preserve_context_on_close);

  // Forwards a `ClearPopup()` call to the page. `callback` is called
  // with the final input text from the page.
  void ClearPopup(base::OnceCallback<void(const std::string&)> callback);

  // Forwards an `AddContext()` call to the page. This call is intended to be
  // used to notify the page that searchbox context has been added.
  void AddContext(std::unique_ptr<SearchboxContextData::Context> context);

  // Forwards a `FocusInput()` call to the page.
  void FocusInput();

 protected:
  virtual OmniboxAimPopupWebUIContent* GetAimPopupContent();

 private:
  mojo::Receiver<omnibox_popup_aim::mojom::PageHandler> receiver_;
  mojo::Remote<omnibox_popup_aim::mojom::Page> page_;
  raw_ptr<content::WebContents> web_contents_;
  base::WeakPtr<TopChromeWebUIController::Embedder> embedder_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_AIM_HANDLER_H_
