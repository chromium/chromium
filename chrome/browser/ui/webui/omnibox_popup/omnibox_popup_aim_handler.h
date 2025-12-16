// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_AIM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_AIM_HANDLER_H_

#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/webui/omnibox_popup/mojom/omnibox_popup_aim.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class OmniboxPopupUI;
class GURL;

class OmniboxPopupAimHandler : public omnibox_popup_aim::mojom::PageHandler {
 public:
  OmniboxPopupAimHandler(
      mojo::PendingReceiver<omnibox_popup_aim::mojom::PageHandler> receiver,
      mojo::PendingRemote<omnibox_popup_aim::mojom::Page> page,
      OmniboxPopupUI* omnibox_popup_ui);

  OmniboxPopupAimHandler(const OmniboxPopupAimHandler&) = delete;
  OmniboxPopupAimHandler& operator=(const OmniboxPopupAimHandler&) = delete;

  ~OmniboxPopupAimHandler() override;

  // omnibox_popup_aim::mojom::PageHandler:
  // Forwards a close event from the page to the browser.
  void RequestClose() override;
  void NavigateCurrentTab(const GURL& url) override;

  // Forwards an `OnWidgetShown()` call to the page.
  void OnPopupShown(std::unique_ptr<SearchboxContextData::Context> context);

  // Sets whether the context should be preserved when the popup is closed. This
  // value is reset to false when the popup is shown again.
  void SetPreserveContextOnClose(bool preserve_context_on_close);

  // Forwards an `OnWidgetClosed()` call to the page.
  void OnPopupHidden();

  // Forwards an `AddContext()` call to the page. This call is intended to be
  // used to notify the page that searchbox context has been added.
  void AddContext(std::unique_ptr<SearchboxContextData::Context> context);

 private:
  void OnPopupHiddenCallback(const std::string& input);

  mojo::Receiver<omnibox_popup_aim::mojom::PageHandler> receiver_;
  mojo::Remote<omnibox_popup_aim::mojom::Page> page_;
  raw_ptr<OmniboxPopupUI> omnibox_popup_ui_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_AIM_HANDLER_H_
