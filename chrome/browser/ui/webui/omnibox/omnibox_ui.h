// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_UI_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_UI_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/omnibox/omnibox.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class OmniboxPageHandler;

#if !defined(OS_ANDROID)
class OmniboxPopupHandler;
#endif

// The UI for chrome://omnibox/
class OmniboxUI : public ui::MojoWebUIController {
 public:
  explicit OmniboxUI(content::WebUI* contents);

  OmniboxUI(const OmniboxUI&) = delete;
  OmniboxUI& operator=(const OmniboxUI&) = delete;

  ~OmniboxUI() override;

  // Instantiates the implementor of the mojom::OmniboxPageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<mojom::OmniboxPageHandler> receiver);

#if !defined(OS_ANDROID)
  // This is needed for the Views native UI to call into the WebUI code.
  OmniboxPopupHandler* popup_handler() { return popup_handler_.get(); }
#endif

 private:
  std::unique_ptr<OmniboxPageHandler> omnibox_handler_;

#if !defined(OS_ANDROID)
  std::unique_ptr<OmniboxPopupHandler> popup_handler_;
#endif

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_UI_H_
