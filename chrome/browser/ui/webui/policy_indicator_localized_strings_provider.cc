// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy_indicator_localized_strings_provider.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#endif

namespace policy_indicator {

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  int controlled_setting_policy_id = IDS_CONTROLLED_SETTING_POLICY;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::DemoSession::IsDeviceInDemoMode())
    controlled_setting_policy_id = IDS_CONTROLLED_SETTING_DEMO_SESSION;
#endif
  webui::LocalizedString localized_strings[] = {
    {"controlledSettingPolicy", controlled_setting_policy_id},
    {"controlledSettingRecommendedMatches", IDS_CONTROLLED_SETTING_RECOMMENDED},
    {"controlledSettingRecommendedDiffers",
     IDS_CONTROLLED_SETTING_HAS_RECOMMENDATION},
    {"controlledSettingExtension", IDS_CONTROLLED_SETTING_EXTENSION},
    {"controlledSettingExtensionWithoutName",
     IDS_CONTROLLED_SETTING_EXTENSION_WITHOUT_NAME},
    {"controlledSettingChildRestriction",
     IDS_CONTROLLED_SETTING_CHILD_RESTRICTION},
    {"controlledSettingParent", IDS_CONTROLLED_SETTING_PARENT},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"controlledSettingShared", IDS_CONTROLLED_SETTING_SHARED},
    {"controlledSettingWithOwner", IDS_CONTROLLED_SETTING_WITH_OWNER},
    {"controlledSettingNoOwner", IDS_CONTROLLED_SETTING_NO_OWNER},
#endif
  };
  html_source->AddLocalizedStrings(localized_strings);
}

}  // namespace policy_indicator
