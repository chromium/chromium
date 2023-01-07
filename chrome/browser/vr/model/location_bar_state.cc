// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/location_bar_state.h"

namespace vr {

LocationBarState::LocationBarState()
    : gurl(GURL()),
      security_level(security_state::SecurityLevel::NONE),
      vector_icon(nullptr),
      should_display_url(true),
      offline_page(false) {}

LocationBarState::LocationBarState(const GURL& url,
                                   security_state::SecurityLevel level,
                                   const gfx::VectorIcon* icon,
                                   bool display_url,
                                   bool offline)
    : gurl(url),
      security_level(level),
      vector_icon(icon),
      should_display_url(display_url),
      offline_page(offline) {}

LocationBarState::LocationBarState(const LocationBarState& other) = default;

LocationBarState& LocationBarState::operator=(const LocationBarState& other) =
    default;

bool LocationBarState::operator==(const LocationBarState& other) const {
  return (gurl == other.gurl && security_level == other.security_level &&
          vector_icon == other.vector_icon &&
          should_display_url == other.should_display_url &&
          offline_page == other.offline_page);
}

bool LocationBarState::operator!=(const LocationBarState& other) const {
  return !(*this == other);
}

}  // namespace vr
