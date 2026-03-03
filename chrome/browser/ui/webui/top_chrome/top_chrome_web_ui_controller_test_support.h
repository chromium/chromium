// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEB_UI_CONTROLLER_TEST_SUPPORT_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEB_UI_CONTROLLER_TEST_SUPPORT_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "ui/gfx/geometry/point.h"

namespace ui {
class MenuModel;
}

class TestEmbedder : public TopChromeWebUIController::Embedder {
 public:
  TestEmbedder();
  virtual ~TestEmbedder();

  TestEmbedder(const TestEmbedder&) = delete;
  TestEmbedder& operator=(const TestEmbedder&) = delete;

  // TopChromeWebUIController::Embedder:
  void ShowUI() override;
  void CloseUI() override;
  void ShowContextMenu(gfx::Point point,
                       std::unique_ptr<ui::MenuModel> menu_model) override;
  void HideContextMenu() override;

  bool ui_shown() const { return ui_shown_; }
  bool ui_closed() const { return ui_closed_; }
  bool context_menu_shown() const { return context_menu_shown_; }
  std::optional<gfx::Point> last_context_menu_point() const {
    return last_context_menu_point_;
  }

  base::WeakPtr<TestEmbedder> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  bool ui_shown_ = false;
  bool ui_closed_ = false;
  bool context_menu_shown_ = false;
  std::optional<gfx::Point> last_context_menu_point_;

  base::WeakPtrFactory<TestEmbedder> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEB_UI_CONTROLLER_TEST_SUPPORT_H_
