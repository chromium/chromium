// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEB_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEB_UI_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace gfx {
class Point;
}

namespace content {
class WebUI;
}  // namespace content

namespace ui {
class MenuModel;
}  // namespace ui

class TopChromeWebUIController : public ui::MojoWebUIController {
 public:
  class Embedder {
   public:
    virtual void ShowUI() = 0;
    virtual void CloseUI() = 0;
    virtual void ShowContextMenu(gfx::Point point,
                                 std::unique_ptr<ui::MenuModel> menu_model) = 0;
    virtual void HideContextMenu() = 0;
  };

  // By default TopChromeWebUIController do not have normal WebUI bindings.
  // Pass |enable_chrome_send| as true if these are needed.
  explicit TopChromeWebUIController(content::WebUI* contents,
                                     bool enable_chrome_send = false);
  TopChromeWebUIController(const TopChromeWebUIController&) = delete;
  TopChromeWebUIController& operator=(const TopChromeWebUIController&) =
      delete;
  ~TopChromeWebUIController() override;

  void set_embedder(base::WeakPtr<Embedder> embedder) { embedder_ = embedder; }
  base::WeakPtr<Embedder> embedder() { return embedder_; }

 private:
  base::WeakPtr<Embedder> embedder_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEB_UI_CONTROLLER_H_
