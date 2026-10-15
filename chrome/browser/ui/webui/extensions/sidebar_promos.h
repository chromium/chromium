// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_SIDEBAR_PROMOS_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_SIDEBAR_PROMOS_H_

#include <optional>

#include "base/containers/span.h"
#include "base/time/time.h"

namespace extensions {

// A message shown to developers in the sidebar of the chrome://extensions
// page. Only one is shown at a time.
struct SidebarPromo {
  // The message to show. It has a single link, whose href is "$1".
  int message_id;

  // Where the link goes.
  const char* url;

  // The UTC date the promo stops being shown. Unset means it never expires.
  std::optional<base::Time::Exploded> expiration;
};

// Returns every promo, including expired ones. They are compile-time
// constants, so the span and each `url` stay valid for the lifetime of the
// process.
base::span<const SidebarPromo> GetSidebarPromos();

// Returns the promo from `promos` to show at `now`, or nullopt if all of them
// have expired. Rotates through the unexpired promos, one per day. Keeps no
// state, so it can be called on any thread.
std::optional<SidebarPromo> PickSidebarPromo(
    base::span<const SidebarPromo> promos,
    base::Time now);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_SIDEBAR_PROMOS_H_
