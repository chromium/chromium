// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/window_info.h"

#include "base/strings/stringprintf.h"

namespace full_restore {

namespace {

std::string ToPrefixedString(base::Optional<int32_t> val,
                             const std::string& prefix) {
  return prefix + base::StringPrintf(": %d \n", val ? *val : -1);
}

std::string ToPrefixedString(base::Optional<bool> val,
                             const std::string& prefix) {
  return prefix + base::StringPrintf(": %d \n", val ? *val : 0);
}

std::string ToPrefixedString(base::Optional<gfx::Rect> val,
                             const std::string& prefix) {
  return prefix + ": " + (val ? *val : gfx::Rect()).ToString() + " \n";
}

std::string ToPrefixedString(base::Optional<chromeos::WindowStateType> val,
                             const std::string& prefix) {
  base::Optional<int> new_val =
      val ? base::make_optional(static_cast<int>(*val)) : base::nullopt;
  return ToPrefixedString(new_val, prefix);
}

}  // namespace

WindowInfo::WindowInfo() = default;
WindowInfo::~WindowInfo() = default;

std::string WindowInfo::ToString() const {
  return ToPrefixedString(activation_index, "Activation index") +
         ToPrefixedString(desk_id, "Desk") +
         ToPrefixedString(visible_on_all_workspaces,
                          "Visible on all workspaces") +
         ToPrefixedString(restore_bounds, "Restore bounds") +
         ToPrefixedString(current_bounds, "Current bounds") +
         ToPrefixedString(window_state_type, "Window state") +
         ToPrefixedString(display_id, "Display id");
}

}  // namespace full_restore
