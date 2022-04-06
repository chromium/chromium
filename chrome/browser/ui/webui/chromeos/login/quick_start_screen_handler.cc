// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/quick_start_screen_handler.h"

#include "base/values.h"
#include "chrome/browser/ash/login/screens/quick_start_screen.h"

namespace chromeos {

constexpr StaticOobeScreenId QuickStartView::kScreenId;

QuickStartScreenHandler::QuickStartScreenHandler()
    : BaseScreenHandler(kScreenId) {
  set_user_acted_method_path_deprecated("login.QuickStartScreen.userActed");
}

QuickStartScreenHandler::~QuickStartScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void QuickStartScreenHandler::Show() {
  if (!IsJavascriptAllowed()) {
    show_on_init_ = true;
    return;
  }

  ShowInWebUI();
}

void QuickStartScreenHandler::Bind(QuickStartScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreenDeprecated(screen_);
  if (IsJavascriptAllowed())
    InitializeDeprecated();
}

void QuickStartScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreenDeprecated(nullptr);
}

std::vector<base::Value> ToValue(const ash::quick_start::ShapeList& list) {
  std::vector<base::Value> result;
  for (const ash::quick_start::ShapeHolder& shape_holder : list) {
    base::flat_map<std::string, base::Value> val;
    val["shape"] = base::Value(static_cast<int>(shape_holder.shape));
    val["color"] = base::Value(static_cast<int>(shape_holder.color));
    val["digit"] = base::Value(static_cast<int>(shape_holder.digit));
    result.emplace_back(std::move(val));
  }
  return result;
}

void QuickStartScreenHandler::SetShapes(
    const ash::quick_start::ShapeList& shape_list) {
  CallJS("login.QuickStartScreen.setFigures", base::Value(ToValue(shape_list)));
}

void QuickStartScreenHandler::InitializeDeprecated() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void QuickStartScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

}  // namespace chromeos
