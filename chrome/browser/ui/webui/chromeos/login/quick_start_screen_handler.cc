// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/quick_start_screen_handler.h"

#include "base/values.h"

namespace chromeos {

QuickStartScreenHandler::QuickStartScreenHandler()
    : BaseScreenHandler(kScreenId) {}

QuickStartScreenHandler::~QuickStartScreenHandler() = default;

void QuickStartScreenHandler::Show() {
  ShowInWebUI();
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
  CallExternalAPI("setFigures", base::Value(ToValue(shape_list)));
}

void QuickStartScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

}  // namespace chromeos
