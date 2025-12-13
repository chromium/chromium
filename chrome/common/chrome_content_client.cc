// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/media/cdm_registration.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/common_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/crash/core/common/crash_key.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/embedder_support/origin_trials/origin_trial_policy_impl.h"
#include "components/heap_profiling/in_process/child_process_snapshot_controller.h"
#include "components/heap_profiling/in_process/heap_profiler_controller.h"
#include "components/heap_profiling/in_process/mojom/snapshot_controller.mojom.h"
#include "components/services/heap_profiling/public/cpp/profiling_client.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/buildflags.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_util.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/http/http_util.h"
#include "pdf/buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "components/webapps/isolated_web_apps/scheme.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <fcntl.h>
#include "sandbox/linux/services/credentials.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/common/constants.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/common/webplugininfo.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/common/constants.h"
#include "components/pdf/common/pdf_util.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "chrome/common/media/cdm_host_file_path.h"
#endif
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/common/media/chrome_media_drm_bridge_client.h"
#endif

namespace {

}  // namespace

ChromeContentClient::ChromeContentClient() = default;

ChromeContentClient::~ChromeContentClient() = default;

void ChromeContentClient::SetActiveURL(const GURL& url,
                                       std::string top_origin) {
  static crash_reporter::CrashKeyString<1024> active_url("url-chunk");
  active_url.Set(url.possibly_invalid_spec());

  // Use a large enough size for Origin::GetDebugString.
  static crash_reporter::CrashKeyString<128> top_origin_key("top-origin");
  top_origin_key.Set(top_origin);
}

void ChromeContentClient::SetGpuInfo(const gpu::GPUInfo& gpu_info) {
  gpu::SetKeysForCrashLogging(gpu_info);
}

void ChromeContentClient::AddPlugins(
    std::vector<content::WebPluginInfo>* plugins) {
#if BUILDFLAG(ENABLE_PDF)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static constexpr char16_t kPDFPluginName[] = u"Chrome PDF Plugin";
#else
  static constexpr char16_t kPDFPluginName[] = u"Chromium PDF Plugin";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static constexpr char16_t kPDFPluginDescription[] = u"Built-in PDF viewer";
  static constexpr char kPDFPluginExtension[] = "pdf";
  static constexpr char kPDFPluginExtensionDescription[] =
      "Portable Document Format";

  content::WebPluginInfo pdf_info;
  pdf_info.name = kPDFPluginName;
  pdf_info.path = base::FilePath(ChromeContentClient::kPDFInternalPluginPath);
  pdf_info.desc = kPDFPluginDescription;
  content::WebPluginMimeType pdf_mime_type(pdf::kInternalPluginMimeType,
                                           kPDFPluginExtension,
                                           kPDFPluginExtensionDescription);
  pdf_info.mime_types.push_back(pdf_mime_type);
  pdf_info.type = content::WebPluginInfo::PLUGIN_TYPE_BROWSER_INTERNAL_PLUGIN;
  plugins->push_back(pdf_info);
#endif  // BUILDFLAG(ENABLE_PDF)
}

void ChromeContentClient::AddContentDecryptionModules(
    std::vector<content::CdmInfo>* cdms,
    std::vector<media::CdmHostFilePath>* cdm_host_file_paths) {
  if (cdms)
    RegisterCdmInfo(cdms);

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  if (cdm_host_file_paths)
    AddCdmHostFilePaths(cdm_host_file_paths);
#endif
#endif
}

// New schemes by which content can be retrieved should almost certainly be
// marked as "standard" schemes, even if they're internal, chrome-only schemes.
// "Standard" here just means that its URLs behave like 'normal' URL do.
//   - Standard schemes get canonicalized like "new-scheme://hostname/[path]"
//   - Whereas "new-scheme:hostname" is a valid nonstandard URL.
//   - Thus, hostnames can't be extracted from non-standard schemes.
//   - The presence of hostnames enables the same-origin policy. Resources like
//     "new-scheme://foo/" are kept separate from "new-scheme://bar/". For
//     a nonstandard scheme, every resource loaded from that scheme could
//     have access to every other resource.
//   - The same-origin policy is very important if webpages can be
//     loaded via the scheme. Try to organize the URL space of any new scheme
//     such that hostnames provide meaningful compartmentalization of
//     privileges.
//
// Example standard schemes: https://, chrome-extension://, chrome://, file://
// Example nonstandard schemes: mailto:, data:, javascript:, about:
//
// Warning: Adding a scheme here will make URLs with that scheme incompatible
// with other parts of the web. If you just need the URL parser to handle the
// hostname or path correctly, you don't need to add a scheme here since
// non-special scheme URLs are now supported (see http://crbug.com/40063064 for
// details). If you add a new scheme, please also add WPT tests for it like
// https://crrev.com/c/5790445.
static const char* const kChromeStandardURLSchemes[] = {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    extensions::kExtensionScheme,
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
    webapps::kIsolatedAppScheme,
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
    chrome::kChromeNativeScheme,        chrome::kChromeSearchScheme,
    dom_distiller::kDomDistillerScheme,
#if BUILDFLAG(IS_ANDROID)
    content::kAndroidAppScheme,
#endif
};

void ChromeContentClient::AddAdditionalSchemes(Schemes* schemes) {
  for (auto* standard_scheme : kChromeStandardURLSchemes)
    schemes->standard_schemes.push_back(standard_scheme);

#if BUILDFLAG(IS_ANDROID)
  schemes->referrer_schemes.push_back(content::kAndroidAppScheme);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  schemes->extension_schemes.push_back(extensions::kExtensionScheme);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  schemes->isolated_app_schemes.push_back(webapps::kIsolatedAppScheme);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  schemes->savable_schemes.push_back(extensions::kExtensionScheme);
#endif
  schemes->savable_schemes.push_back(chrome::kChromeSearchScheme);
  schemes->savable_schemes.push_back(dom_distiller::kDomDistillerScheme);

  // chrome-search: resources shouldn't trigger insecure content warnings.
  schemes->secure_schemes.push_back(chrome::kChromeSearchScheme);

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Treat extensions as secure because communication with them is entirely in
  // the browser, so there is no danger of manipulation or eavesdropping on
  // communication with them by third parties.
  schemes->secure_schemes.push_back(extensions::kExtensionScheme);
#endif

  // chrome-native: is a scheme used for placeholder navigations that allow
  // UIs to be drawn with platform native widgets instead of HTML.  These pages
  // should be treated as empty documents that can commit synchronously.
  schemes->empty_document_schemes.push_back(chrome::kChromeNativeScheme);
  schemes->no_access_schemes.push_back(chrome::kChromeNativeScheme);
  schemes->secure_schemes.push_back(chrome::kChromeNativeScheme);

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  schemes->service_worker_schemes.push_back(extensions::kExtensionScheme);
  schemes->service_worker_schemes.push_back(url::kFileScheme);

  // As far as Blink is concerned, they should be allowed to receive CORS
  // requests. At the Extensions layer, requests will actually be blocked unless
  // overridden by the web_accessible_resources manifest key.
  // TODO(kalman): See what happens with a service worker.
  schemes->cors_enabled_schemes.push_back(extensions::kExtensionScheme);

  schemes->csp_bypassing_schemes.push_back(extensions::kExtensionScheme);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  schemes->predefined_handler_schemes.emplace_back(
      url::kMailToScheme, chrome::kChromeOSDefaultMailtoHandler);
  schemes->predefined_handler_schemes.emplace_back(
      url::kWebcalScheme, chrome::kChromeOSDefaultWebcalHandler);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  schemes->secure_schemes.push_back(webapps::kIsolatedAppScheme);
  schemes->cors_enabled_schemes.push_back(webapps::kIsolatedAppScheme);
  schemes->service_worker_schemes.push_back(webapps::kIsolatedAppScheme);
  url::AddWebStorageScheme(webapps::kIsolatedAppScheme);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
  schemes->local_schemes.push_back(content::kExternalFileScheme);
#endif

#if BUILDFLAG(IS_ANDROID)
  schemes->local_schemes.push_back(url::kContentScheme);
#endif
}

std::u16string ChromeContentClient::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

std::u16string ChromeContentClient::GetLocalizedString(
    int message_id,
    const std::u16string& replacement) {
  return l10n_util::GetStringFUTF16(message_id, replacement);
}

bool ChromeContentClient::HasDataResource(int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().HasDataResource(resource_id);
}

std::string_view ChromeContentClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ChromeContentClient::GetDataResourceBytes(
    int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

std::string ChromeContentClient::GetDataResourceString(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      resource_id);
}

gfx::Image& ChromeContentClient::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

std::string ChromeContentClient::GetProcessTypeNameInEnglish(int type) {
  // TODO(crbug.com/423859723): Remove this method.
  NOTREACHED() << "Unknown child process type!";
}

blink::OriginTrialPolicy* ChromeContentClient::GetOriginTrialPolicy() {
  // Prevent initialization race (see crbug.com/721144). There may be a
  // race when the policy is needed for worker startup (which happens on a
  // separate worker thread).
  base::AutoLock auto_lock(origin_trial_policy_lock_);
  if (!origin_trial_policy_)
    origin_trial_policy_ =
        std::make_unique<embedder_support::OriginTrialPolicyImpl>();
  return origin_trial_policy_.get();
}

#if BUILDFLAG(IS_ANDROID)
media::MediaDrmBridgeClient* ChromeContentClient::GetMediaDrmBridgeClient() {
  return new ChromeMediaDrmBridgeClient();
}
#endif  // BUILDFLAG(IS_ANDROID)

void ChromeContentClient::ExposeInterfacesToBrowser(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    mojo::BinderMap* binders) {
  // Sets up the client side of the multi-process heap profiler service.
  // TODO(crbug.com/40915258): Hook up chrome://memory-internals to the
  // in-process heap profiler, and delete this service.
  binders
      ->Add<heap_profiling::mojom::ProfilingClient>(

          [](mojo::PendingReceiver<heap_profiling::mojom::ProfilingClient>
                 receiver) {
            static base::NoDestructor<heap_profiling::ProfilingClient>
                profiling_client;
            profiling_client->BindToInterface(std::move(receiver));
          },
          io_task_runner);

  // Sets up the simplified in-process heap profiler, if it's enabled.
  const auto* heap_profiler_controller =
      heap_profiling::HeapProfilerController::GetInstance();
  if (heap_profiler_controller && heap_profiler_controller->IsEnabled()) {
    binders->Add<heap_profiling::mojom::SnapshotController>(
        &heap_profiling::ChildProcessSnapshotController::
            CreateSelfOwnedReceiver,
        // ChildProcessSnapshotController calls into HeapProfilerController,
        // which can only be accessed on this sequence.
        base::SequencedTaskRunner::GetCurrentDefault());
  }
}

bool ChromeContentClient::IsFilePickerAllowedForCrossOriginSubframe(
    const url::Origin& origin) {
#if BUILDFLAG(ENABLE_PDF)
  return IsPdfExtensionOrigin(origin);
#else
  return false;
#endif
}
