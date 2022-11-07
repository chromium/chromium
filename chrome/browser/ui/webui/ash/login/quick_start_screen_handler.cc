// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"

#include "base/values.h"

namespace ash {

QuickStartScreenHandler::QuickStartScreenHandler()
    : BaseScreenHandler(kScreenId) {}

QuickStartScreenHandler::~QuickStartScreenHandler() = default;

void QuickStartScreenHandler::Show() {
  ShowInWebUI();
}

base::Value ToValue(const quick_start::ShapeList& list) {
  base::Value::List result;
  for (const quick_start::ShapeHolder& shape_holder : list) {
    base::Value::Dict val;
    val.Set("shape", static_cast<int>(shape_holder.shape));
    val.Set("color", static_cast<int>(shape_holder.color));
    val.Set("digit", static_cast<int>(shape_holder.digit));
    result.Append(std::move(val));
  }
  return base::Value(std::move(result));
}

void QuickStartScreenHandler::SetShapes(
    const quick_start::ShapeList& shape_list) {
  CallExternalAPI("setFigures", ToValue(shape_list));
}

void QuickStartScreenHandler::SetQRCode(base::Value::List blob) {
  CallExternalAPI("setQRCode", std::move(blob));
}

void QuickStartScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

}  // namespace ash
