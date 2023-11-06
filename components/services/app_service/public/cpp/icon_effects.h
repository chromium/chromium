// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_EFFECTS_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_EFFECTS_H_

#include <cstdint>

namespace apps {
// A bitwise-or of icon post-processing effects.
//
// It derives from a uint32_t because it needs to be the same size as the
// uint32_t IconKey.icon_effects field.

// This enum is used to mask the icon_effects value in crosapi, which is a
// stable interface that needs to be backwards compatible. Do not change the
// masks here.
enum IconEffects : uint32_t {
  kNone = 0x00,

  // The icon effects are applied in numerical order, low to high. It is always
  // resize-and-then-badge and never badge-and-then-resize, which can matter if
  // the badge has a fixed size.
  kMdIconStyle = 0x01,   // Icon should have Material Design style. Resize and
                         // add padding if necessary.
  kChromeBadge = 0x02,   // Another (Android) app has the same name.
  kBlocked = 0x04,       // Disabled apps are grayed out and badged.
  kRoundCorners = 0x08,  // Bookmark apps get round corners.
  kPaused = 0x10,  // Paused apps are grayed out and badged to indicate they
                   // cannot be launched.
  kCrOsStandardBackground =
      0x40,                   // Add the white background to the standard icon.
  kCrOsStandardMask = 0x80,   // Apply the mask to the standard icon.
  kCrOsStandardIcon = 0x100,  // Add the white background, maybe shrink the
                              // icon, and apply the mask to the standard icon
                              // This effect combines kCrOsStandardBackground
                              // and kCrOsStandardMask together.
  kGuestOsBadge = 0x200,      // Badge used to identify Crostini apps.
};

inline IconEffects operator|(IconEffects a, IconEffects b) {
  return static_cast<IconEffects>(static_cast<uint32_t>(a) |
                                  static_cast<uint32_t>(b));
}

inline IconEffects operator|=(IconEffects& a, IconEffects b) {
  a = a | b;
  return a;
}

inline IconEffects operator&(IconEffects a, uint32_t b) {
  return static_cast<IconEffects>(static_cast<uint32_t>(a) &
                                  static_cast<uint32_t>(b));
}

inline IconEffects operator&=(IconEffects& a, uint32_t b) {
  a = a & b;
  return a;
}

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_EFFECTS_H_
