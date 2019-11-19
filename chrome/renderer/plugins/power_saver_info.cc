// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/renderer/plugins/power_saver_info.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "url/origin.h"

namespace {

std::string GetPluginInstancePosterAttribute(
    const blink::WebPluginParams& params) {
  DCHECK_EQ(params.attribute_names.size(), params.attribute_values.size());

  for (size_t i = 0; i < params.attribute_names.size(); ++i) {
    if (params.attribute_names[i].Utf8() == "poster" &&
        !params.attribute_values[i].IsEmpty()) {
      return params.attribute_values[i].Utf8();
    }
  }
  return std::string();
}

bool GetPluginPowerSaverEnabled(bool power_saver_setting_on, bool is_flash) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string override_for_testing = command_line->GetSwitchValueASCII(
      switches::kOverridePluginPowerSaverForTesting);

  // This feature has only been tested thoroughly with Flash thus far. It is
  // also enabled for the Power Saver test plugin for browser tests.
  if (override_for_testing == "always")
    return true;
  else if (override_for_testing == "never")
    return false;
  else
    return power_saver_setting_on && is_flash;
}

}  // namespace

PowerSaverInfo::PowerSaverInfo()
    : power_saver_enabled(false), blocked_for_background_tab(false) {}

PowerSaverInfo::PowerSaverInfo(const PowerSaverInfo& other) = default;

PowerSaverInfo PowerSaverInfo::Get(content::RenderFrame* render_frame,
                                   bool power_saver_setting_on,
                                   const blink::WebPluginParams& params,
                                   const content::WebPluginInfo& plugin_info,
                                   const GURL& document_url) {
  bool is_flash =
      plugin_info.name == base::ASCIIToUTF16(content::kFlashPluginName);

  PowerSaverInfo info;
  info.power_saver_enabled =
      GetPluginPowerSaverEnabled(power_saver_setting_on, is_flash);

  if (info.power_saver_enabled) {
    info.blocked_for_background_tab = render_frame->IsHidden();
    info.poster_attribute = GetPluginInstancePosterAttribute(params);
    info.base_url = document_url;
  }

  return info;
}
