// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/accessibility/accessibility_service_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chromecast/ui/display_settings_manager.h"

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>
#include "base/android/jni_android.h"
#include "chromecast/browser/jni_headers/CastAccessibilityHelper_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/browser/accessibility/accessibility_manager.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/common/extensions_api/accessibility_private.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {
constexpr char kExtensionsDirDefault[] = "/system/chrome/extensions";
constexpr char kChromeVoxManifestFile[] = "chromevox_manifest.json";
}  // namespace
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)

namespace chromecast {
namespace shell {

AccessibilityServiceImpl::AccessibilityServiceImpl(
    content::BrowserContext* browser_context,
    DisplaySettingsManager* display_settings_manager)
    : browser_context_(browser_context),
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
      display_settings_manager_(display_settings_manager),
      installer_task_runner_(extensions::GetExtensionFileTaskRunner())
#else
      display_settings_manager_(display_settings_manager)
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
{
  DCHECK(browser_context);
}

AccessibilityServiceImpl::~AccessibilityServiceImpl() = default;

void AccessibilityServiceImpl::SetColorInversion(bool enable) {
  if (enable != color_inversion_enabled_) {
    NotifyAccessibilitySettingChanged(kColorInversion, enable);
  }

  color_inversion_enabled_ = enable;
  display_settings_manager_->SetColorInversion(enable);
}

bool AccessibilityServiceImpl::IsScreenReaderEnabled() {
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  return (chromevox_extension_ != nullptr);
#elif BUILDFLAG(IS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_CastAccessibilityHelper_isScreenReaderEnabled(env);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
}

bool AccessibilityServiceImpl::IsMagnificationGestureEnabled() {
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  return CastBrowserProcess::GetInstance()
      ->accessibility_manager()
      ->IsMagnificationGestureEnabled();
#else
  return false;
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
}

void AccessibilityServiceImpl::NotifyAccessibilitySettingChanged(
    AccessibilitySettingType type,
    bool new_value) {
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  std::vector<CastWebContents*>& webviews =
      chromecast::CastWebContents::GetAll();

  // Tell all renderers the setting has changed.
  for (chromecast::CastWebContents* webview : webviews) {
    mojo::Remote<mojom::CastAccessibilityClient> accessibility_client;
    content::RenderFrameHost* render_frame_host =
        webview->web_contents()->GetPrimaryMainFrame();

    if (!render_frame_host)
      continue;

    if (!render_frame_host->IsRenderFrameLive())
      continue;

    service_manager::InterfaceProvider* interface_provider =
        render_frame_host->GetRemoteInterfaces();

    if (!interface_provider)
      continue;

    interface_provider->GetInterface(
        accessibility_client.BindNewPipeAndPassReceiver());
    switch (type) {
      case kScreenReader:
        accessibility_client->ScreenReaderSettingChanged(new_value);
        break;
      case kColorInversion:
        accessibility_client->ColorInversionSettingChanged(new_value);
        break;
      case kMagnificationGesture:
        accessibility_client->MagnificationGestureSettingChanged(new_value);
        break;
    }
  }
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
}

void AccessibilityServiceImpl::SetScreenReader(bool enable) {
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  if (enable == chromevox_enabled_) {
    LOG(WARNING) << "Screen reader already "
                 << (enable ? "enabled" : "disabled");
    return;
  }

  NotifyAccessibilitySettingChanged(kScreenReader, enable);

  chromevox_enabled_ = enable;
  CastBrowserProcess::GetInstance()->accessibility_manager()->SetScreenReader(
      enable);

  // Disable extension.
  if (!enable) {
    // Only disable automation here. Enabling must be deferred until after
    // automation is fully enabled by the extension we're about to load.
    // Otherwise, the first tree we generate will go nowhere. See
    // AutomationManagerAura::Enable() for where that happens.
    CastBrowserProcess::GetInstance()->AccessibilityStateChanged(false);

    extensions::CastExtensionSystem* extension_system =
        static_cast<extensions::CastExtensionSystem*>(
            extensions::ExtensionSystem::Get(browser_context_));

    extension_system->UnloadExtension(
        chromevox_extension_->id(),
        extensions::UnloadedExtensionReason::UNINSTALL);
    chromevox_extension_ = nullptr;

    CastBrowserProcess::GetInstance()->accessibility_manager()->HideFocusRing();

    // Disable accessibility mode in all web contents.
    auto all_web_contents = chromecast::CastWebContents::GetAll();
    for (CastWebContents* web_contents : all_web_contents) {
      web_contents->web_contents()->SetAccessibilityMode(0);
    }

    return;
  }

  extensions::CastExtensionSystem* extension_system =
      static_cast<extensions::CastExtensionSystem*>(
          extensions::ExtensionSystem::Get(browser_context_));

  // Enable extension.
  if (!installer_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&AccessibilityServiceImpl::LoadChromeVoxExtension,
                         base::Unretained(this), extension_system)))
    NOTREACHED();
#else
  LOG(ERROR) << "SetScreenReader is not supported on this platform";
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
}

void AccessibilityServiceImpl::SetMagnificationGestureEnabled(bool enable) {
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  if (magnify_gesture_enabled_ != enable) {
    NotifyAccessibilitySettingChanged(kMagnificationGesture, enable);
  }
  magnify_gesture_enabled_ = enable;

  CastBrowserProcess::GetInstance()
      ->accessibility_manager()
      ->SetMagnificationGestureEnabled(enable);
#else
  LOG(ERROR)
      << "SetMagnificationGestureEnabled is not supported on this platform";
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
}

void AccessibilityServiceImpl::GetAccessibilitySettings(
    mojom::CastAccessibilityService::GetAccessibilitySettingsCallback
        callback) {
  DVLOG(2) << __func__;

  auto settings = mojom::AccessibilitySettings::New();
  settings->color_inversion_enabled = color_inversion_enabled_;
  settings->screen_reader_enabled = IsScreenReaderEnabled();
  settings->magnification_gesture_enabled = IsMagnificationGestureEnabled();

  std::move(callback).Run(std::move(settings));
}

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
void AccessibilityServiceImpl::LoadChromeVoxExtension(
    extensions::CastExtensionSystem* extension_system) {
  std::string ext_dir =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kExtensionsDir);
  if (ext_dir.empty()) {
    ext_dir = kExtensionsDirDefault;
  }

  chromevox_extension_ = extension_system->LoadExtension(
      kChromeVoxManifestFile, base::FilePath(ext_dir));

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&AccessibilityServiceImpl::AnnounceChromeVox,
                                base::Unretained(this)));
}
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)

void AccessibilityServiceImpl::AnnounceChromeVox() {
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(browser_context_);

  const std::string& extension_id = extension_misc::kChromeVoxExtensionId;

  base::Value::List event_args;
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_INTRODUCE_CHROME_VOX,
      extensions::cast::api::accessibility_private::OnIntroduceChromeVox::
          kEventName,
      std::move(event_args)));
  event_router->DispatchEventWithLazyListener(extension_id, std::move(event));
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
}

}  // namespace shell
}  // namespace chromecast
