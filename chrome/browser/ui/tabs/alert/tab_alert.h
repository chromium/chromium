// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ALERT_TAB_ALERT_H_
#define CHROME_BROWSER_UI_TABS_ALERT_TAB_ALERT_H_

namespace tabs {
// Alert states for a tab. Any number of these (or none) may apply at once.
// LINT.IfChange(TabAlert)
enum class TabAlert {
  kMediaRecording,       // Audio/Video [both] being recorded, consumed by tab.
  kTabCapturing,         // Tab contents being captured.
  kAudioPlaying,         // Audible audio is playing from the tab.
  kAudioMuting,          // Tab audio is being muted.
  kBluetoothConnected,   // Tab is connected to a BT Device.
  kBluetoothScanActive,  // Tab is actively scanning for BT devices.
  kUsbConnected,         // Tab is connected to a USB device.
  kHidConnected,         // Tab is connected to a HID device.
  kSerialConnected,      // Tab is connected to a serial device.
  kPipPlaying,           // Tab contains a video in Picture-in-Picture mode.
  kDesktopCapturing,     // Desktop contents being recorded, consumed by tab.
  kVrPresentingInHeadset,  // VR content is being presented in a headset.
  kAudioRecording,         // Audio [only] being recorded, consumed by tab.
  kVideoRecording,         // Video [only] being recorded, consumed by tab.
  kGlicAccessing,          // Glic is accessing the tab's contents.
  kGlicSharing,            // The tab's contents are shared with glic.
  kActorAccessing,         // Actor is accessing the tab's contents.
  kActorWaitingOnUser,     // Actor is waiting on user to respond.
};
// Any changes to the TabAlert enum needs to be updated in CompareAlerts as
// well.
// LINT.ThenChange(/chrome/browser/ui/tabs/alert/tab_alert_controller.cc)
}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_ALERT_TAB_ALERT_H_
