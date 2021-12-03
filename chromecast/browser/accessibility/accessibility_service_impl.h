// Copyright 2021 The Chromium Authors. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ACCESSIBILITY_ACCESSIBILITY_SERVICE_IMPL_H_
#define CHROMECAST_BROWSER_ACCESSIBILITY_ACCESSIBILITY_SERVICE_IMPL_H_

#include "chromecast/chromecast_buildflags.h"
#include "chromecast/common/mojom/accessibility.mojom.h"

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
#include "chromecast/browser/extensions/cast_extension_system.h"
#include "extensions/common/extension.h"
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace chromecast {

class CastWindowManager;
class DisplaySettingsManager;

namespace shell {

// Browser side mojo service provider for accessibility functions.
// One instance per browser process.
class AccessibilityServiceImpl : public mojom::CastAccessibilityService {
 public:
  AccessibilityServiceImpl(content::BrowserContext* browser_context,
                           DisplaySettingsManager* display_settings_manager);
  ~AccessibilityServiceImpl() override;
  AccessibilityServiceImpl(const AccessibilityServiceImpl&) = delete;
  AccessibilityServiceImpl& operator=(const AccessibilityServiceImpl&) = delete;

  // mojom::CastAccessibilityService implementation:
  void SetColorInversion(bool enable) override;
  void SetScreenReader(bool enable) override;
  void SetMagnificationGestureEnabled(bool enable) override;
  void GetAccessibilitySettings(
      GetAccessibilitySettingsCallback callback) override;

  void Stop();

 private:
  enum AccessibilitySettingType {
    kScreenReader,
    kColorInversion,
    kMagnificationGesture,
  };

  void NotifyAccessibilitySettingChanged(AccessibilitySettingType type,
                                         bool new_value);

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  void LoadChromeVoxExtension(
      extensions::CastExtensionSystem* extension_system);
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  void AnnounceChromeVox();
  bool IsScreenReaderEnabled();
  bool IsMagnificationGestureEnabled();

  content::BrowserContext* const browser_context_;
  DisplaySettingsManager* const display_settings_manager_;

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  bool chromevox_enabled_ = false;
  const extensions::Extension* chromevox_extension_ = nullptr;
  const scoped_refptr<base::SequencedTaskRunner> installer_task_runner_;
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)

  bool color_inversion_enabled_ = false;
  bool magnify_gesture_enabled_ = false;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ACCESSIBILITY_ACCESSIBILITY_SERVICE_IMPL_H_
