// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/window_info.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace app_restore {

namespace {

std::string ToPrefixedString(std::optional<int32_t> val,
                             const std::string& prefix) {
  return prefix + base::StringPrintf(": %d \n", val.value_or(-1));
}

std::string ToPrefixedString(std::optional<gfx::Rect> val,
                             const std::string& prefix) {
  return prefix + ": " + val.value_or(gfx::Rect()).ToString() + " \n";
}

std::string ToPrefixedString(std::optional<chromeos::WindowStateType> val,
                             const std::string& prefix) {
  std::optional<int> new_val =
      val ? std::make_optional(static_cast<int32_t>(*val)) : std::nullopt;
  return ToPrefixedString(new_val, prefix);
}

std::string ToPrefixedString(std::optional<ui::mojom::WindowShowState> val,
                             const std::string& prefix) {
  std::optional<int> new_val =
      val ? std::make_optional(static_cast<int32_t>(*val)) : std::nullopt;
  return ToPrefixedString(new_val, prefix);
}

std::string ToPrefixedString(std::optional<std::u16string> val,
                             const std::string& prefix) {
  return prefix + ": " + base::UTF16ToASCII(val.value_or(u""));
}

std::string ToPrefixedString(base::Uuid val, const std::string& prefix) {
  return prefix + ": " + val.AsLowercaseString() + " \n";
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
  return ToPrefixedString(activation_index, "Activation index") +
         ToPrefixedString(desk_id, "Desk") +
         ToPrefixedString(desk_guid, "Desk guid") +
         ToPrefixedString(current_bounds, "Current bounds") +
         ToPrefixedString(window_state_type, "Window state") +
         ToPrefixedString(pre_minimized_show_state_type,
                          "Pre minimized show state") +
         ToPrefixedString(snap_percentage, "Snap percentage") +
         ToPrefixedString(display_id, "Display id") +
         ToPrefixedString(app_title, "App Title");
}

}  // namespace app_restore
