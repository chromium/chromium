// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_WEBUI_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_WEBUI_CONSTANTS_H_

#include <string_view>

namespace ash::kiosk_vision {

inline constexpr std::string_view kChromeUIKioskVisionInternalsURL =
    "chrome://kiosk-vision-internals";
inline constexpr std::string_view kChromeUIKioskVisionInternalsHost =
    "kiosk-vision-internals";

}  // namespace ash::kiosk_vision

#endif  // CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_WEBUI_CONSTANTS_H_
