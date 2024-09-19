// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/chrome_extensions_renderer_api_provider.h"

#include "build/chromeos_buildflags.h"
#include "chrome/grit/renderer_resources.h"
#include "chrome/renderer/extensions/api/app_hooks_delegate.h"
#include "chrome/renderer/extensions/api/extension_hooks_delegate.h"
#include "chrome/renderer/extensions/api/identity_hooks_delegate.h"
#include "chrome/renderer/extensions/api/media_galleries_custom_bindings.h"
#include "chrome/renderer/extensions/api/notifications_native_handler.h"
#include "chrome/renderer/extensions/api/page_capture_custom_bindings.h"
#include "chrome/renderer/extensions/api/sync_file_system_custom_bindings.h"
#include "chrome/renderer/extensions/api/tabs_hooks_delegate.h"
#include "extensions/renderer/bindings/api_bindings_system.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/lazy_background_page_native_handler.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_handler.h"
#include "extensions/renderer/resource_bundle_source_map.h"
#include "extensions/renderer/script_context.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/renderer/extensions/api/file_browser_handler_custom_bindings.h"
#include "chrome/renderer/extensions/api/platform_keys_natives.h"
#if BUILDFLAG(USE_CUPS)
#include "chrome/renderer/extensions/api/printing_hooks_delegate.h"
#endif
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/renderer/extensions/api/accessibility_private_hooks_delegate.h"
#include "chrome/renderer/extensions/api/file_manager_private_custom_bindings.h"
#endif

namespace extensions {

void ChromeExtensionsRendererAPIProvider::RegisterNativeHandlers(
    ModuleSystem* module_system,
    NativeExtensionBindingsSystem* bindings_system,
    V8SchemaRegistry* v8_schema_registry,
    ScriptContext* context) const {
  module_system->RegisterNativeHandler(
      "sync_file_system",
      std::make_unique<SyncFileSystemCustomBindings>(context));
#if BUILDFLAG(IS_CHROMEOS)
  module_system->RegisterNativeHandler(
      "file_browser_handler",
      std::make_unique<FileBrowserHandlerCustomBindings>(context));
  module_system->RegisterNativeHandler(
      "platform_keys_natives", std::make_unique<PlatformKeysNatives>(context));
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  module_system->RegisterNativeHandler(
      "file_manager_private",
      std::make_unique<FileManagerPrivateCustomBindings>(context));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  module_system->RegisterNativeHandler(
      "notifications_private",
      std::make_unique<NotificationsNativeHandler>(context));
  module_system->RegisterNativeHandler(
      "mediaGalleries",
      std::make_unique<MediaGalleriesCustomBindings>(context));
  module_system->RegisterNativeHandler(
      "page_capture", std::make_unique<PageCaptureCustomBindings>(
                          context, bindings_system->GetIPCMessageSender()));

  // The following are native handlers that are defined in //extensions, but
  // are only used for APIs defined in Chrome.
  // TODO(devlin): We should clean this up. If an API is defined in Chrome,
  // there's no reason to have its native handlers residing and being compiled
  // in //extensions.
  module_system->RegisterNativeHandler(
      "lazy_background_page",
      std::make_unique<LazyBackgroundPageNativeHandler>(context));
}

void ChromeExtensionsRendererAPIProvider::AddBindingsSystemHooks(
    Dispatcher* dispatcher,
    NativeExtensionBindingsSystem* bindings_system) const {
  APIBindingsSystem* bindings = bindings_system->api_system();
  bindings->RegisterHooksDelegate(
      "app", std::make_unique<extensions::AppHooksDelegate>(
                 dispatcher, bindings->request_handler(),
                 bindings_system->GetIPCMessageSender()));
  bindings->RegisterHooksDelegate(
      "extension", std::make_unique<extensions::ExtensionHooksDelegate>(
                       bindings_system->messaging_service()));
  bindings->RegisterHooksDelegate(
      "tabs", std::make_unique<extensions::TabsHooksDelegate>(
                  bindings_system->messaging_service()));
  bindings->RegisterHooksDelegate(
      "identity", std::make_unique<extensions::IdentityHooksDelegate>());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bindings->RegisterHooksDelegate(
      "accessibilityPrivate",
      std::make_unique<extensions::AccessibilityPrivateHooksDelegate>());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
  bindings->RegisterHooksDelegate(
      "printing", std::make_unique<extensions::PrintingHooksDelegate>());
#endif
}

void ChromeExtensionsRendererAPIProvider::PopulateSourceMap(
    ResourceBundleSourceMap* source_map) const {
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

#if BUILDFLAG(IS_CHROMEOS)
  source_map->RegisterSource("certificateProvider",
                             IDR_CERTIFICATE_PROVIDER_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("enterprise.platformKeys",
                             IDR_ENTERPRISE_PLATFORM_KEYS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("enterprise.platformKeys.CryptoKey",
                             IDR_ENTERPRISE_PLATFORM_KEYS_CRYPTO_KEY_JS);
  source_map->RegisterSource("enterprise.platformKeys.SubtleCrypto",
                             IDR_ENTERPRISE_PLATFORM_KEYS_SUBTLE_CRYPTO_JS);
  source_map->RegisterSource("enterprise.platformKeys.Token",
                             IDR_ENTERPRISE_PLATFORM_KEYS_TOKEN_JS);
  source_map->RegisterSource("fileBrowserHandler",
                             IDR_FILE_BROWSER_HANDLER_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("fileSystemProvider",
                             IDR_FILE_SYSTEM_PROVIDER_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("platformKeys",
                             IDR_PLATFORM_KEYS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("platformKeys.getCryptoKeyUtil",
                             IDR_PLATFORM_KEYS_GET_CRYPTO_KEY_UTIL_JS);
  source_map->RegisterSource("platformKeys.Key", IDR_PLATFORM_KEYS_KEY_JS);
  source_map->RegisterSource("platformKeys.SubtleCrypto",
                             IDR_PLATFORM_KEYS_SUBTLE_CRYPTO_JS);
  source_map->RegisterSource("platformKeys.utils", IDR_PLATFORM_KEYS_UTILS_JS);

  // Remote Apps.
  source_map->RegisterSource("chromeos.remote_apps.mojom-lite",
                             IDR_REMOTE_APPS_MOJOM_LITE_JS);
  source_map->RegisterSource("chromeos.remote_apps",
                             IDR_REMOTE_APPS_BINDINGS_JS);
  source_map->RegisterSource("url/mojom/url.mojom-lite",
                             IDR_MOJO_URL_MOJOM_LITE_JS);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  source_map->RegisterSource("fileManagerPrivate",
                             IDR_FILE_MANAGER_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("terminalPrivate",
                             IDR_TERMINAL_PRIVATE_CUSTOM_BINDINGS_JS);

  // IME service on Chrome OS.
  source_map->RegisterSource("ash.ime.mojom.ime_service.mojom",
                             IDR_IME_SERVICE_MOJOM_JS);
  source_map->RegisterSource("ash.ime.mojom.input_engine.mojom",
                             IDR_IME_SERVICE_INPUT_ENGINE_MOJOM_JS);
  source_map->RegisterSource("ash.ime.mojom.input_method.mojom",
                             IDR_IME_SERVICE_INPUT_METHOD_MOJOM_JS);
  source_map->RegisterSource("ash.ime.mojom.input_method_host.mojom",
                             IDR_IME_SERVICE_INPUT_METHOD_HOST_MOJOM_JS);
  source_map->RegisterSource("chromeos.ime.service",
                             IDR_IME_SERVICE_BINDINGS_JS);

  source_map->RegisterSource("chromeos.tts.mojom.google_tts_stream.mojom",
                             IDR_GOOGLE_TTS_STREAM_MOJOM_JS);
  source_map->RegisterSource("chromeos.tts.google_stream",
                             IDR_GOOGLE_TTS_STREAM_BINDINGS_JS);

  source_map->RegisterSource("ash.enhanced_network_tts.mojom-lite",
                             IDR_ENHANCED_NETWORK_TTS_MOJOM_LITE_JS);
  source_map->RegisterSource("ash.enhanced_network_tts",
                             IDR_ENHANCED_NETWORK_TTS_BINDINGS_JS);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  source_map->RegisterSource(
      "webrtcDesktopCapturePrivate",
      IDR_WEBRTC_DESKTOP_CAPTURE_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("webrtcLoggingPrivate",
                             IDR_WEBRTC_LOGGING_PRIVATE_CUSTOM_BINDINGS_JS);

  // Platform app sources that are not API-specific..
  source_map->RegisterSource("chromeWebViewElement",
                             IDR_CHROME_WEB_VIEW_ELEMENT_JS);
  source_map->RegisterSource("chromeWebViewInternal",
                             IDR_CHROME_WEB_VIEW_INTERNAL_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("chromeWebView", IDR_CHROME_WEB_VIEW_JS);
}

void ChromeExtensionsRendererAPIProvider::EnableCustomElementAllowlist() const {
}

void ChromeExtensionsRendererAPIProvider::RequireWebViewModules(
    ScriptContext* context) const {
  DCHECK(context->GetAvailability("webViewInternal").is_available());
  if (context->GetAvailability("chromeWebViewTag").is_available()) {
    // CHECK that the Chrome WebView and Controlled Frame features aren't both
    // enabled in the same context. This is here because Controlled Frame
    // is based on WebView and modifies base classes in order to not ship some
    // APIs. These modifications could harm a live WebView instance if we
    // allowed both in a single instance, but these features aren't designed
    // to be enabled in the same instance. This check confirms that is held.
    CHECK(!context->GetAvailability("controlledFrameInternal").is_available());

    context->module_system()->Require("chromeWebViewElement");
  }
}

}  // namespace extensions
