// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_extensions_dispatcher_delegate.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/grit/renderer_resources.h"
#include "chrome/renderer/extensions/accessibility_private_hooks_delegate.h"
#include "chrome/renderer/extensions/app_hooks_delegate.h"
#include "chrome/renderer/extensions/cast_streaming_native_handler.h"
#include "chrome/renderer/extensions/extension_hooks_delegate.h"
#include "chrome/renderer/extensions/media_galleries_custom_bindings.h"
#include "chrome/renderer/extensions/notifications_native_handler.h"
#include "chrome/renderer/extensions/page_capture_custom_bindings.h"
#include "chrome/renderer/extensions/sync_file_system_custom_bindings.h"
#include "chrome/renderer/extensions/tabs_hooks_delegate.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/bindings/api_bindings_system.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/lazy_background_page_native_handler.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_handler.h"
#include "extensions/renderer/resource_bundle_source_map.h"
#include "extensions/renderer/script_context.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_security_policy.h"

#if defined(OS_CHROMEOS)
#include "chrome/renderer/extensions/file_browser_handler_custom_bindings.h"
#include "chrome/renderer/extensions/file_manager_private_custom_bindings.h"
#include "chrome/renderer/extensions/platform_keys_natives.h"
#endif

using extensions::NativeHandler;

ChromeExtensionsDispatcherDelegate::ChromeExtensionsDispatcherDelegate() {}

ChromeExtensionsDispatcherDelegate::~ChromeExtensionsDispatcherDelegate() {}

void ChromeExtensionsDispatcherDelegate::RegisterNativeHandlers(
    extensions::Dispatcher* dispatcher,
    extensions::ModuleSystem* module_system,
    extensions::NativeExtensionBindingsSystem* bindings_system,
    extensions::ScriptContext* context) {
  module_system->RegisterNativeHandler(
      "sync_file_system",
      std::unique_ptr<NativeHandler>(
          new extensions::SyncFileSystemCustomBindings(context)));
#if defined(OS_CHROMEOS)
  module_system->RegisterNativeHandler(
      "file_browser_handler",
      std::unique_ptr<NativeHandler>(
          new extensions::FileBrowserHandlerCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "file_manager_private",
      std::unique_ptr<NativeHandler>(
          new extensions::FileManagerPrivateCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "platform_keys_natives",
      std::unique_ptr<NativeHandler>(
          new extensions::PlatformKeysNatives(context)));
#endif  // defined(OS_CHROMEOS)
  module_system->RegisterNativeHandler(
      "notifications_private",
      std::unique_ptr<NativeHandler>(
          new extensions::NotificationsNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "mediaGalleries",
      std::unique_ptr<NativeHandler>(
          new extensions::MediaGalleriesCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "page_capture", std::unique_ptr<NativeHandler>(
                          new extensions::PageCaptureCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "cast_streaming_natives",
      std::make_unique<extensions::CastStreamingNativeHandler>(
          context, bindings_system));

  // The following are native handlers that are defined in //extensions, but
  // are only used for APIs defined in Chrome.
  // TODO(devlin): We should clean this up. If an API is defined in Chrome,
  // there's no reason to have its native handlers residing and being compiled
  // in //extensions.
  module_system->RegisterNativeHandler(
      "lazy_background_page",
      std::unique_ptr<NativeHandler>(
          new extensions::LazyBackgroundPageNativeHandler(context)));
}

void ChromeExtensionsDispatcherDelegate::PopulateSourceMap(
    extensions::ResourceBundleSourceMap* source_map) {
  // Custom bindings.
  source_map->RegisterSource("action", IDR_ACTION_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("browserAction",
                             IDR_BROWSER_ACTION_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("declarativeContent",
                             IDR_DECLARATIVE_CONTENT_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("desktopCapture",
                             IDR_DESKTOP_CAPTURE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("developerPrivate",
                             IDR_DEVELOPER_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("downloads", IDR_DOWNLOADS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("gcm", IDR_GCM_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("identity", IDR_IDENTITY_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("imageWriterPrivate",
                             IDR_IMAGE_WRITER_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("input.ime", IDR_INPUT_IME_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("mediaGalleries",
                             IDR_MEDIA_GALLERIES_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("notifications",
                             IDR_NOTIFICATIONS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("omnibox", IDR_OMNIBOX_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("pageAction", IDR_PAGE_ACTION_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("pageCapture",
                             IDR_PAGE_CAPTURE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("syncFileSystem",
                             IDR_SYNC_FILE_SYSTEM_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("systemIndicator",
                             IDR_SYSTEM_INDICATOR_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("tabCapture", IDR_TAB_CAPTURE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("tts", IDR_TTS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("ttsEngine", IDR_TTS_ENGINE_CUSTOM_BINDINGS_JS);

#if defined(OS_CHROMEOS)
  source_map->RegisterSource("certificateProvider",
                             IDR_CERTIFICATE_PROVIDER_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("enterprise.platformKeys",
                             IDR_ENTERPRISE_PLATFORM_KEYS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("enterprise.platformKeys.internalAPI",
                             IDR_ENTERPRISE_PLATFORM_KEYS_INTERNAL_API_JS);
  source_map->RegisterSource("enterprise.platformKeys.KeyPair",
                             IDR_ENTERPRISE_PLATFORM_KEYS_KEY_PAIR_JS);
  source_map->RegisterSource("enterprise.platformKeys.SubtleCrypto",
                             IDR_ENTERPRISE_PLATFORM_KEYS_SUBTLE_CRYPTO_JS);
  source_map->RegisterSource("enterprise.platformKeys.Token",
                             IDR_ENTERPRISE_PLATFORM_KEYS_TOKEN_JS);
  source_map->RegisterSource("fileBrowserHandler",
                             IDR_FILE_BROWSER_HANDLER_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("fileManagerPrivate",
                             IDR_FILE_MANAGER_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("fileSystemProvider",
                             IDR_FILE_SYSTEM_PROVIDER_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("platformKeys",
                             IDR_PLATFORM_KEYS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("platformKeys.getPublicKey",
                             IDR_PLATFORM_KEYS_GET_PUBLIC_KEY_JS);
  source_map->RegisterSource("platformKeys.internalAPI",
                             IDR_PLATFORM_KEYS_INTERNAL_API_JS);
  source_map->RegisterSource("platformKeys.Key", IDR_PLATFORM_KEYS_KEY_JS);
  source_map->RegisterSource("platformKeys.SubtleCrypto",
                             IDR_PLATFORM_KEYS_SUBTLE_CRYPTO_JS);
  source_map->RegisterSource("platformKeys.utils", IDR_PLATFORM_KEYS_UTILS_JS);
  source_map->RegisterSource("terminalPrivate",
                             IDR_TERMINAL_PRIVATE_CUSTOM_BINDINGS_JS);

  // IME service on Chrome OS.
  source_map->RegisterSource("chromeos.ime.mojom.input_engine.mojom",
                             IDR_IME_SERVICE_MOJOM_JS);
  source_map->RegisterSource("chromeos.ime.service",
                             IDR_IME_SERVICE_BINDINGS_JS);
#endif  // defined(OS_CHROMEOS)

  source_map->RegisterSource("cast.streaming.rtpStream",
                             IDR_CAST_STREAMING_RTP_STREAM_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("cast.streaming.session",
                             IDR_CAST_STREAMING_SESSION_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource(
      "cast.streaming.udpTransport",
      IDR_CAST_STREAMING_UDP_TRANSPORT_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource(
      "cast.streaming.receiverSession",
      IDR_CAST_STREAMING_RECEIVER_SESSION_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource(
      "webrtcDesktopCapturePrivate",
      IDR_WEBRTC_DESKTOP_CAPTURE_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("webrtcLoggingPrivate",
                             IDR_WEBRTC_LOGGING_PRIVATE_CUSTOM_BINDINGS_JS);

  // Platform app sources that are not API-specific..
  source_map->RegisterSource("chromeWebViewInternal",
                             IDR_CHROME_WEB_VIEW_INTERNAL_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("chromeWebView", IDR_CHROME_WEB_VIEW_JS);

  // Media router.
  source_map->RegisterSource(
      "chrome/common/media_router/mojom/media_controller.mojom",
      IDR_MEDIA_CONTROLLER_MOJOM_JS);
  source_map->RegisterSource(
      "chrome/common/media_router/mojom/media_router.mojom",
      IDR_MEDIA_ROUTER_MOJOM_JS);
  source_map->RegisterSource(
      "chrome/common/media_router/mojom/media_status.mojom",
      IDR_MEDIA_STATUS_MOJOM_JS);
  source_map->RegisterSource("media_router_bindings",
                             IDR_MEDIA_ROUTER_BINDINGS_JS);
  source_map->RegisterSource("mojo/public/mojom/base/time.mojom",
                             IDR_MOJO_TIME_MOJOM_JS);
  source_map->RegisterSource("mojo/public/mojom/base/unguessable_token.mojom",
                             IDR_MOJO_UNGUESSABLE_TOKEN_MOJOM_JS);
  source_map->RegisterSource("net/interfaces/ip_address.mojom",
                             IDR_MOJO_IP_ADDRESS_MOJOM_JS);
  source_map->RegisterSource("net/interfaces/ip_endpoint.mojom",
                             IDR_MOJO_IP_ENDPOINT_MOJOM_JS);
  source_map->RegisterSource("url/mojom/origin.mojom", IDR_ORIGIN_MOJOM_JS);
  source_map->RegisterSource("url/mojom/url.mojom", IDR_MOJO_URL_MOJOM_JS);
  source_map->RegisterSource("media/mojo/mojom/remoting_common.mojom",
                             IDR_REMOTING_COMMON_JS);
  source_map->RegisterSource(
      "media/mojo/mojom/mirror_service_remoting.mojom",
      IDR_MEDIA_REMOTING_JS);
  source_map->RegisterSource(
      "components/mirroring/mojom/mirroring_service_host.mojom",
      IDR_MIRRORING_SERVICE_HOST_MOJOM_JS);
  source_map->RegisterSource(
      "components/mirroring/mojom/cast_message_channel.mojom",
      IDR_MIRRORING_CAST_MESSAGE_CHANNEL_MOJOM_JS);
  source_map->RegisterSource(
      "components/mirroring/mojom/session_observer.mojom",
      IDR_MIRRORING_SESSION_OBSERVER_MOJOM_JS);
  source_map->RegisterSource(
      "components/mirroring/mojom/session_parameters.mojom",
      IDR_MIRRORING_SESSION_PARAMETERS_JS);
}

void ChromeExtensionsDispatcherDelegate::RequireWebViewModules(
    extensions::ScriptContext* context) {
  DCHECK(context->GetAvailability("webViewInternal").is_available());
  context->module_system()->Require("chromeWebView");
}

void ChromeExtensionsDispatcherDelegate::OnActiveExtensionsUpdated(
    const std::set<std::string>& extension_ids) {
  // In single-process mode, the browser process reports the active extensions.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kSingleProcess))
    return;
  crash_keys::SetActiveExtensions(extension_ids);
}

void ChromeExtensionsDispatcherDelegate::InitializeBindingsSystem(
    extensions::Dispatcher* dispatcher,
    extensions::NativeExtensionBindingsSystem* bindings_system) {
  extensions::APIBindingsSystem* bindings = bindings_system->api_system();
  bindings->GetHooksForAPI("app")->SetDelegate(
      std::make_unique<extensions::AppHooksDelegate>(
          dispatcher, bindings->request_handler()));
  bindings->GetHooksForAPI("extension")
      ->SetDelegate(std::make_unique<extensions::ExtensionHooksDelegate>(
          bindings_system->messaging_service()));
  bindings->GetHooksForAPI("tabs")->SetDelegate(
      std::make_unique<extensions::TabsHooksDelegate>(
          bindings_system->messaging_service()));
  bindings->GetHooksForAPI("accessibilityPrivate")
      ->SetDelegate(
          std::make_unique<extensions::AccessibilityPrivateHooksDelegate>());
}
