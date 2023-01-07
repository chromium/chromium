// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_MEDIA_CAPS_H_
#define CHROMECAST_MEDIA_BASE_MEDIA_CAPS_H_

#include "ui/gfx/geometry/size.h"

namespace chromecast {
namespace media {

// The MediaCapabitlies is a convenience API which makes various audio/video
// capabitlies exposed by AvSettings available both in the browser and
// renderer processes in the Chrome sandboxed model (the AvSettings API is
// currently only available in the browser process).
class MediaCapabilities {
 public:
  static void ScreenResolutionChanged(const gfx::Size& res);
  static void ScreenInfoChanged(int hdcp_version,
                                int supported_eotfs,
                                int dolby_vision_flags,
                                int screen_width_mm,
                                int screen_height_mm,
                                bool cur_mode_supports_hdr,
                                bool cur_mode_supports_dv);
  // HDCP version multiplied by 10, e.g. 22 means HDCP 2.2.
  static int GetHdcpVersion();
  // HdmiSinkSupports* functions check for display support independent of
  // current HDMI mode.
  static bool HdmiSinkSupportsEOTF_SDR();
  static bool HdmiSinkSupportsEOTF_HDR();
  static bool HdmiSinkSupportsEOTF_SMPTE_ST_2084();
  static bool HdmiSinkSupportsEOTF_HLG();
  static bool HdmiSinkSupportsDolbyVision();
  static bool HdmiSinkSupportsDolbyVision_4K_p60();
  static bool HdmiSinkSupportsDolbyVision_422_12bit();
  // Check for HDR support (i.e. >= 10-bit) and DV support in current HDMI
  // mode.
  static bool CurrentHdmiModeSupportsHDR();
  static bool CurrentHdmiModeSupportsDolbyVision();
  static gfx::Size GetScreenResolution();

 private:
  static unsigned int g_hdmi_codecs;
  static int g_hdcp_version;
  static int g_supported_eotfs;
  static int g_dolby_vision_flags;
  static bool g_cur_mode_supports_hdr;
  static bool g_cur_mode_supports_dv;
  static gfx::Size g_screen_resolution;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_MEDIA_CAPS_H_
