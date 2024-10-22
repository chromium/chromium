// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/window_info.h"

#include <sstream>

#include "base/values.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace app_restore {

namespace {

std::string WindowStateTypeToString(
    std::optional<chromeos::WindowStateType> val) {
  if (!val) {
    return std::string();
  }
  std::stringstream stream;
  stream << *val;
  return stream.str();
}

std::string WindowShowStateToString(
    std::optional<ui::mojom::WindowShowState> val) {
  if (!val) {
    return std::string();
  }
  return WindowStateTypeToString(chromeos::ToWindowStateType(*val));
}

}  // namespace

BrowserExtraInfo::BrowserExtraInfo() = default;

BrowserExtraInfo::BrowserExtraInfo(BrowserExtraInfo&& other) = default;

BrowserExtraInfo::BrowserExtraInfo(const BrowserExtraInfo&) = default;

BrowserExtraInfo& BrowserExtraInfo::operator=(BrowserExtraInfo&& other) =
    default;

BrowserExtraInfo& BrowserExtraInfo::operator=(const BrowserExtraInfo&) =
    default;

BrowserExtraInfo::~BrowserExtraInfo() = default;

bool BrowserExtraInfo::operator==(const BrowserExtraInfo& other) const =
    default;

WindowInfo::WindowInfo() = default;

WindowInfo::WindowInfo(WindowInfo&& other) = default;

WindowInfo::WindowInfo(const WindowInfo&) = default;

WindowInfo& WindowInfo::operator=(WindowInfo&& other) = default;

WindowInfo& WindowInfo::operator=(const WindowInfo&) = default;

WindowInfo::~WindowInfo() = default;

bool WindowInfo::operator==(const WindowInfo& other) const = default;

std::string WindowInfo::ToString() const {
  auto root = base::Value::Dict().Set(
      "Window Info",
      base::Value::Dict()
          .Set("Activation index", activation_index.value_or(-1))
          .Set("Desk", desk_id.value_or(-1))
          .Set("Desk guid", desk_guid.AsLowercaseString())
          .Set("Current bounds",
               current_bounds.value_or(gfx::Rect()).ToString())
          .Set("Window state type", WindowStateTypeToString(window_state_type))
          .Set("Pre minimized show state",
               WindowShowStateToString(pre_minimized_show_state_type))
          .Set("Snap percentage", static_cast<int>(snap_percentage.value_or(0)))
          .Set("Display id", static_cast<int>(display_id.value_or(-1)))
          .Set("App title", app_title.value_or(std::u16string())));
  return root.DebugString();
}

}  // namespace app_restore
