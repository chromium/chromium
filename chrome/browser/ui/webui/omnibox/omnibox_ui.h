// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_UI_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_UI_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/omnibox/omnibox.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class OmniboxPageHandler;

// The UI for chrome://omnibox/
class OmniboxUI : public ui::MojoWebUIController {
 public:
  explicit OmniboxUI(content::WebUI* contents);
  ~OmniboxUI() override;

 private:
  void BindOmniboxPageHandler(
      mojo::PendingReceiver<mojom::OmniboxPageHandler> receiver);

  std::unique_ptr<OmniboxPageHandler> omnibox_handler_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_UI_H_
