// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller_test_support.h"

#include "ui/base/models/menu_model.h"

TestEmbedder::TestEmbedder() = default;
TestEmbedder::~TestEmbedder() = default;

void TestEmbedder::ShowUI() {
  ui_shown_ = true;
}

void TestEmbedder::CloseUI() {
  ui_closed_ = true;
}

void TestEmbedder::ShowContextMenu(gfx::Point point,
                                   std::unique_ptr<ui::MenuModel> menu_model) {
  context_menu_shown_ = true;
  last_context_menu_point_ = point;
}

void TestEmbedder::HideContextMenu() {
  context_menu_shown_ = false;
}
