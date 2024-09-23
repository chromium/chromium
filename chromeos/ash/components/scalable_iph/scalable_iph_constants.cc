// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"

namespace scalable_iph {

std::ostream& operator<<(std::ostream& out, ActionType action_type) {
  switch (action_type) {
    case ActionType::kInvalid:
      return out << "Invalid";
    case ActionType::kOpenChrome:
      return out << "OpenChrome";
    case ActionType::kOpenLauncher:
      return out << "OpenLauncher";
    case ActionType::kOpenPersonalizationApp:
      return out << "OpenPersonalizationApp";
    case ActionType::kOpenPlayStore:
      return out << "OpenPlayStore";
    case ActionType::kOpenGoogleDocs:
      return out << "OpenGoogleDocs";
    case ActionType::kOpenGooglePhotos:
      return out << "OpenGooglePhotos";
    case ActionType::kOpenSettingsPrinter:
      return out << "OpenSettingsPrinter";
    case ActionType::kOpenPhoneHub:
      return out << "OpenPhoneHub";
    case ActionType::kOpenYouTube:
      return out << "OpenYouTube";
    case ActionType::kOpenFileManager:
      return out << "OpenFileManager";
    case ActionType::kOpenHelpAppPerks:
      return out << "OpenHelpAppPerks";
    case ActionType::kOpenChromebookPerksWeb:
      return out << "OpenChromebookPerksWeb";
    case ActionType::kOpenChromebookPerksGfnPriority2022:
      return out << "OpenChromebookPerksGfnPriority2022";
    case ActionType::kOpenChromebookPerksMinecraft2023:
      return out << "OpenChromebookPerksMinecraft2023";
    case ActionType::kOpenChromebookPerksMinecraftRealms2023:
      return out << "OpenChromebookPerksMinecraftRealms2023";
  }
}

}  // namespace scalable_iph
