// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/media_caps.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/public/avsettings.h"

namespace chromecast {
namespace media {

int MediaCapabilities::g_hdcp_version = 0;
int MediaCapabilities::g_supported_eotfs = 0;
int MediaCapabilities::g_dolby_vision_flags = 0;
bool MediaCapabilities::g_cur_mode_supports_hdr = false;
bool MediaCapabilities::g_cur_mode_supports_dv = false;
gfx::Size MediaCapabilities::g_screen_resolution(0, 0);

void MediaCapabilities::ScreenResolutionChanged(const gfx::Size& res) {
  LOG(INFO) << __FUNCTION__ << " resolution=" << res.ToString();
  g_screen_resolution = res;

  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  const auto& kHeightSwitch = switches::kForceMediaResolutionHeight;
  int height = 0;
  if (cmd_line->HasSwitch(kHeightSwitch) &&
      base::StringToInt(cmd_line->GetSwitchValueASCII(kHeightSwitch),
                        &height) &&
      height > 0) {
    LOG(INFO) << __func__ << ": overridden screen height=" << height;
    g_screen_resolution.set_height(height);
  }
  const auto& kWidthSwitch = switches::kForceMediaResolutionWidth;
  int width = 0;
  if (cmd_line->HasSwitch(kWidthSwitch) &&
      base::StringToInt(cmd_line->GetSwitchValueASCII(kWidthSwitch), &width) &&
      width > 0) {
    LOG(INFO) << __func__ << ": overridden screen width=" << width;
    g_screen_resolution.set_width(width);
  }
}

void MediaCapabilities::ScreenInfoChanged(int hdcp_version,
                                          int supported_eotfs,
                                          int dolby_vision_flags,
                                          int screen_width_mm,
                                          int screen_height_mm,
                                          bool cur_mode_supports_hdr,
                                          bool cur_mode_supports_dv) {
  LOG(INFO) << __FUNCTION__ << " HDCP version=" << hdcp_version
            << " Supported EOTFs=" << supported_eotfs
            << " DolbyVision flags=" << dolby_vision_flags
            << " Screen size(mm): " << screen_width_mm << "x"
            << screen_height_mm << " cur mode HDR=" << cur_mode_supports_hdr
            << " cur mode DV=" << cur_mode_supports_dv;
  g_hdcp_version = hdcp_version;
  g_supported_eotfs = supported_eotfs;
  g_dolby_vision_flags = dolby_vision_flags;
  g_cur_mode_supports_hdr = cur_mode_supports_hdr;
  g_cur_mode_supports_dv = cur_mode_supports_dv;
}

int MediaCapabilities::GetHdcpVersion() {
  return g_hdcp_version;
}

bool MediaCapabilities::HdmiSinkSupportsEOTF_SDR() {
  return g_supported_eotfs & AvSettings::EOTF_SDR;
}

bool MediaCapabilities::HdmiSinkSupportsEOTF_HDR() {
  return g_supported_eotfs & AvSettings::EOTF_HDR;
}

bool MediaCapabilities::HdmiSinkSupportsEOTF_SMPTE_ST_2084() {
  return g_supported_eotfs & AvSettings::EOTF_SMPTE_ST_2084;
}

bool MediaCapabilities::HdmiSinkSupportsEOTF_HLG() {
  return g_supported_eotfs & AvSettings::EOTF_HLG;
}

bool MediaCapabilities::HdmiSinkSupportsDolbyVision() {
  return g_dolby_vision_flags & AvSettings::DOLBY_SUPPORTED;
}

bool MediaCapabilities::HdmiSinkSupportsDolbyVision_4K_p60() {
  return g_dolby_vision_flags & AvSettings::DOLBY_4K_P60_SUPPORTED;
}

bool MediaCapabilities::HdmiSinkSupportsDolbyVision_422_12bit() {
  return g_dolby_vision_flags & AvSettings::DOLBY_422_12BIT_SUPPORTED;
}

bool MediaCapabilities::CurrentHdmiModeSupportsHDR() {
  return g_cur_mode_supports_hdr;
}

bool MediaCapabilities::CurrentHdmiModeSupportsDolbyVision() {
  return g_cur_mode_supports_dv;
}

gfx::Size MediaCapabilities::GetScreenResolution() {
  return g_screen_resolution;
}

}  // namespace media
}  // namespace chromecast
