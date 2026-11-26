// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_indicator_localized_strings_provider.h"

#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/webui_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#endif

namespace policy_indicator {

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
#if !BUILDFLAG(IS_CHROMEOS)
      {"controlledSettingPolicy", IDS_SETTINGS_ACCOUNT_SYNC_DISABLED},
#endif
      {"controlledSettingRecommendedMatches",
       IDS_CONTROLLED_SETTING_RECOMMENDED},
      {"controlledSettingRecommendedDiffers",
       IDS_CONTROLLED_SETTING_HAS_RECOMMENDATION},
      {"controlledSettingExtension", IDS_CONTROLLED_SETTING_EXTENSION},
      {"controlledSettingExtensionWithoutName",
       IDS_CONTROLLED_SETTING_EXTENSION_WITHOUT_NAME},
      {"controlledSettingChildRestriction",
       IDS_CONTROLLED_SETTING_CHILD_RESTRICTION},
      {"controlledSettingParent", IDS_CONTROLLED_SETTING_PARENT},

#if BUILDFLAG(IS_CHROMEOS)
      {"controlledSettingShared", IDS_CONTROLLED_SETTING_SHARED},
      {"controlledSettingWithOwner", IDS_CONTROLLED_SETTING_WITH_OWNER},
      {"controlledSettingNoOwner", IDS_CONTROLLED_SETTING_NO_OWNER},
#endif
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
#if BUILDFLAG(IS_CHROMEOS)
  html_source->AddLocalizedString("controlledSettingPolicy",
                                  ash::demo_mode::IsDeviceInDemoMode()
                                      ? IDS_CONTROLLED_SETTING_DEMO_SESSION
                                      : IDS_CONTROLLED_SETTING_POLICY);
#endif
}

}  // namespace policy_indicator
