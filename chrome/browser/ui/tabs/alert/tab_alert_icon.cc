// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/alert/tab_alert_icon.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/common/chrome_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#endif

namespace tabs {

ui::ColorId GetAlertIndicatorColor(TabAlert state,
                                   bool is_tab_active,
                                   bool is_frame_active) {
  int group = 0;
  switch (state) {
    case tabs::TabAlert::kMediaRecording:
    case tabs::TabAlert::kAudioRecording:
    case tabs::TabAlert::kVideoRecording:
    case tabs::TabAlert::kDesktopCapturing:
      group = 0;
      break;
    case tabs::TabAlert::kTabCapturing:
    case tabs::TabAlert::kPipPlaying:
    case tabs::TabAlert::kGlicAccessing:
    case tabs::TabAlert::kGlicSharing:
    case tabs::TabAlert::kActorWaitingOnUser:
    case tabs::TabAlert::kActorAccessing:
      group = 1;
      break;
    case tabs::TabAlert::kAudioPlaying:
    case tabs::TabAlert::kAudioMuting:
    case tabs::TabAlert::kBluetoothConnected:
    case tabs::TabAlert::kBluetoothScanActive:
    case tabs::TabAlert::kUsbConnected:
    case tabs::TabAlert::kHidConnected:
    case tabs::TabAlert::kSerialConnected:
    case tabs::TabAlert::kVrPresentingInHeadset:
      group = 2;
      break;
    default:
      NOTREACHED() << "Unknown tab alert state";
  }

  static constexpr std::array<std::array<std::array<ui::ColorId, 2>, 2>, 3>
      color_ids{{{{{kColorTabAlertMediaRecordingInactiveFrameInactive,
                    kColorTabAlertMediaRecordingInactiveFrameActive},
                   {kColorTabAlertMediaRecordingActiveFrameInactive,
                    kColorTabAlertMediaRecordingActiveFrameActive}}},
                 {{{kColorTabAlertPipPlayingInactiveFrameInactive,
                    kColorTabAlertPipPlayingInactiveFrameActive},
                   {kColorTabAlertPipPlayingActiveFrameInactive,
                    kColorTabAlertPipPlayingActiveFrameActive}}},
                 {{{kColorTabAlertAudioPlayingInactiveFrameInactive,
                    kColorTabAlertAudioPlayingInactiveFrameActive},
                   {kColorTabAlertAudioPlayingActiveFrameInactive,
                    kColorTabAlertAudioPlayingActiveFrameActive}}}}};
  return color_ids[group][is_tab_active][is_frame_active];
}

const gfx::VectorIcon& GetAlertIcon(TabAlert alert_state) {
  switch (alert_state) {
    case TabAlert::kAudioPlaying:
      return vector_icons::kVolumeUpChromeRefreshIcon;
    case TabAlert::kAudioMuting:
      return vector_icons::kVolumeOffChromeRefreshIcon;
    case TabAlert::kMediaRecording:
    case TabAlert::kAudioRecording:
    case TabAlert::kVideoRecording:
    case TabAlert::kDesktopCapturing:
      return vector_icons::kRadioButtonCheckedIcon;
    case TabAlert::kTabCapturing:
      return vector_icons::kCaptureIcon;
    case TabAlert::kBluetoothConnected:
      return vector_icons::kBluetoothConnectedIcon;
    case TabAlert::kBluetoothScanActive:
      return vector_icons::kBluetoothScanningChromeRefreshIcon;
    case TabAlert::kUsbConnected:
      return vector_icons::kUsbChromeRefreshIcon;
    case TabAlert::kHidConnected:
      return vector_icons::kVideogameAssetChromeRefreshIcon;
    case TabAlert::kSerialConnected:
      return vector_icons::kSerialPortChromeRefreshIcon;
    case TabAlert::kPipPlaying:
      return vector_icons::kPictureInPictureAltIcon;
    case TabAlert::kVrPresentingInHeadset:
      return vector_icons::kCardboardIcon;
    case TabAlert::kActorWaitingOnUser:
    case TabAlert::kActorAccessing:
    case TabAlert::kGlicAccessing:
    case TabAlert::kGlicSharing:
#if BUILDFLAG(ENABLE_GLIC)
      return glic::GlicVectorIconManager::GetVectorIcon(
          IDR_GLIC_ACCESSING_ICON);
#else
      return kTvIcon;
#endif
  }
}

ui::ImageModel GetAlertImageModel(TabAlert alert_state,
                                  ui::ColorId icon_color) {
  // Tab capturing icon uses a different width compared to
  // the other tab alert indicator icons.
  const int image_width =
      GetLayoutConstant(alert_state == tabs::TabAlert::kTabCapturing
                            ? TAB_ALERT_INDICATOR_CAPTURE_ICON_WIDTH
                            : TAB_ALERT_INDICATOR_ICON_WIDTH);

  return ui::ImageModel::FromVectorIcon(GetAlertIcon(alert_state), icon_color,
                                        image_width);
}

}  // namespace tabs
