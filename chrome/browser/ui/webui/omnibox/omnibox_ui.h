// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_UI_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_UI_H_

#include "build/build_config.h"
#include "chrome/browser/ui/webui/omnibox/omnibox.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class OmniboxPageHandler;

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

 private:
  std::unique_ptr<OmniboxPageHandler> omnibox_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_UI_H_
