// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/window_info.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace app_restore {

namespace {

std::string ToPrefixedString(absl::optional<int32_t> val,
                             const std::string& prefix) {
  return prefix + base::StringPrintf(": %d \n", val ? *val : -1);
}

std::string ToPrefixedString(absl::optional<gfx::Rect> val,
                             const std::string& prefix) {
  return prefix + ": " + (val ? *val : gfx::Rect()).ToString() + " \n";
}

std::string ToPrefixedString(absl::optional<chromeos::WindowStateType> val,
                             const std::string& prefix) {
  absl::optional<int> new_val =
      val ? absl::make_optional(static_cast<int>(*val)) : absl::nullopt;
  return ToPrefixedString(new_val, prefix);
}

std::string ToPrefixedString(absl::optional<ui::WindowShowState> val,
                             const std::string& prefix) {
  absl::optional<int> new_val =
      val ? absl::make_optional(static_cast<int>(*val)) : absl::nullopt;
  return ToPrefixedString(new_val, prefix);
}

std::string ToPrefixedString(absl::optional<std::u16string> val,
                             const std::string& prefix) {
  return prefix + ": " + base::UTF16ToASCII(val.value_or(u""));
}

}  // namespace

WindowInfo::ArcExtraInfo::ArcExtraInfo() = default;
WindowInfo::ArcExtraInfo::ArcExtraInfo(const WindowInfo::ArcExtraInfo&) =
    default;
WindowInfo::ArcExtraInfo& WindowInfo::ArcExtraInfo::operator=(
    const WindowInfo::ArcExtraInfo&) = default;
WindowInfo::ArcExtraInfo::~ArcExtraInfo() = default;

WindowInfo::WindowInfo() = default;
WindowInfo::~WindowInfo() = default;

WindowInfo* WindowInfo::Clone() {
  WindowInfo* new_window_info = new WindowInfo();

  new_window_info->window = window;
  new_window_info->activation_index = activation_index;
  new_window_info->desk_id = desk_id;
  new_window_info->current_bounds = current_bounds;
  new_window_info->window_state_type = window_state_type;
  new_window_info->pre_minimized_show_state_type =
      pre_minimized_show_state_type;
  new_window_info->snap_percentage = snap_percentage;
  new_window_info->display_id = display_id;
  new_window_info->app_title = app_title;
  new_window_info->arc_extra_info = arc_extra_info;
  return new_window_info;
}

std::string WindowInfo::ToString() const {
  return ToPrefixedString(activation_index, "Activation index") +
         ToPrefixedString(desk_id, "Desk") +
         ToPrefixedString(current_bounds, "Current bounds") +
         ToPrefixedString(window_state_type, "Window state") +
         ToPrefixedString(pre_minimized_show_state_type,
                          "Pre minimized show state") +
         ToPrefixedString(snap_percentage, "Snap percentage") +
         ToPrefixedString(display_id, "Display id") +
         ToPrefixedString(app_title, "App Title");
}

}  // namespace app_restore
