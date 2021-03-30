// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/native_library.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/child_process_host_flags.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/common_resources.h"
#include "components/crash/core/common/crash_key.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/embedder_support/origin_trials/origin_trial_policy_impl.h"
#include "components/services/heap_profiling/public/cpp/profiling_client.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_util.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/http/http_util.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/url_constants.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include <fcntl.h>
#include "sandbox/linux/services/credentials.h"
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#include "components/nacl/common/nacl_process_type.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/common/pepper_plugin_info.h"
#include "ppapi/shared_impl/ppapi_permissions.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/cdm/cdm_paths.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_WIDEVINE) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
#include "base/no_destructor.h"
#include "components/cdm/common/cdm_manifest.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
// TODO(crbug.com/663554): Needed for WIDEVINE_CDM_VERSION_STRING. Support
// component updated CDM on all desktop platforms and remove this.
// This file is In SHARED_INTERMEDIATE_DIR.
#include "widevine_cdm_version.h"  // nogncheck
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/common/media/component_widevine_cdm_hint_file_linux.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(ENABLE_WIDEVINE) && (defined(OS_LINUX) ||
        // defined(OS_CHROMEOS))

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "chrome/common/media/cdm_host_file_path.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/common/media/chrome_media_drm_bridge_client.h"
#include "components/embedder_support/android/common/url_constants.h"
#endif

namespace {

#if BUILDFLAG(ENABLE_PLUGINS)
#if BUILDFLAG(ENABLE_PDF)
const char kPDFPluginExtension[] = "pdf";
const char kPDFPluginDescription[] = "Portable Document Format";
const char kPDFPluginOutOfProcessMimeType[] =
    "application/x-google-chrome-pdf";
const uint32_t kPDFPluginPermissions = ppapi::PERMISSION_PDF |
                                       ppapi::PERMISSION_DEV;

content::PepperPluginInfo::GetInterfaceFunc g_pdf_get_interface;
content::PepperPluginInfo::PPP_InitializeModuleFunc g_pdf_initialize_module;
content::PepperPluginInfo::PPP_ShutdownModuleFunc g_pdf_shutdown_module;
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_NACL)
content::PepperPluginInfo::GetInterfaceFunc g_nacl_get_interface;
content::PepperPluginInfo::PPP_InitializeModuleFunc g_nacl_initialize_module;
content::PepperPluginInfo::PPP_ShutdownModuleFunc g_nacl_shutdown_module;
#endif

// Appends the known built-in plugins to the given vector. Some built-in
// plugins are "internal" which means they are compiled into the Chrome binary,
// and some are extra shared libraries distributed with the browser (these are
// not marked internal, aside from being automatically registered, they're just
// regular plugins).
void ComputeBuiltInPlugins(std::vector<content::PepperPluginInfo>* plugins) {
#if BUILDFLAG(ENABLE_PDF)
  content::PepperPluginInfo pdf_info;
  pdf_info.is_internal = true;
  pdf_info.is_out_of_process = true;
  pdf_info.name = ChromeContentClient::kPDFInternalPluginName;
  pdf_info.description = kPDFPluginDescription;
  pdf_info.path = base::FilePath(ChromeContentClient::kPDFPluginPath);
  content::WebPluginMimeType pdf_mime_type(
      kPDFPluginOutOfProcessMimeType,
      kPDFPluginExtension,
      kPDFPluginDescription);
  pdf_info.mime_types.push_back(pdf_mime_type);
  pdf_info.internal_entry_points.get_interface = g_pdf_get_interface;
  pdf_info.internal_entry_points.initialize_module = g_pdf_initialize_module;
  pdf_info.internal_entry_points.shutdown_module = g_pdf_shutdown_module;
  pdf_info.permissions = kPDFPluginPermissions;
  plugins->push_back(pdf_info);
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_NACL)
  // Handle Native Client just like the PDF plugin. This means that it is
  // enabled by default for the non-portable case.  This allows apps installed
  // from the Chrome Web Store to use NaCl even if the command line switch
  // isn't set.  For other uses of NaCl we check for the command line switch.
  content::PepperPluginInfo nacl;
  // The nacl plugin is now built into the Chromium binary.
  nacl.is_internal = true;
  nacl.path = base::FilePath(ChromeContentClient::kNaClPluginFileName);
  nacl.name = nacl::kNaClPluginName;
  content::WebPluginMimeType nacl_mime_type(nacl::kNaClPluginMimeType,
                                            nacl::kNaClPluginExtension,
                                            nacl::kNaClPluginDescription);
  nacl.mime_types.push_back(nacl_mime_type);
  content::WebPluginMimeType pnacl_mime_type(nacl::kPnaclPluginMimeType,
                                             nacl::kPnaclPluginExtension,
                                             nacl::kPnaclPluginDescription);
  nacl.mime_types.push_back(pnacl_mime_type);
  nacl.internal_entry_points.get_interface = g_nacl_get_interface;
  nacl.internal_entry_points.initialize_module = g_nacl_initialize_module;
  nacl.internal_entry_points.shutdown_module = g_nacl_shutdown_module;
  nacl.permissions = ppapi::PERMISSION_PRIVATE | ppapi::PERMISSION_DEV;
  plugins->push_back(nacl);
#endif  // BUILDFLAG(ENABLE_NACL)
}
#endif  //  BUILDFLAG(ENABLE_PLUGINS)

#if (BUILDFLAG(BUNDLE_WIDEVINE_CDM) ||            \
     BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)) && \
    (defined(OS_LINUX) || defined(OS_CHROMEOS))
// Create a CdmInfo for a Widevine CDM, using |version|, |cdm_library_path|, and
// |capability|.
std::unique_ptr<content::CdmInfo> CreateWidevineCdmInfo(
    const base::Version& version,
    const base::FilePath& cdm_library_path,
    content::CdmCapability capability) {
  return std::make_unique<content::CdmInfo>(
      kWidevineCdmDisplayName, kWidevineCdmGuid, version, cdm_library_path,
      kWidevineCdmFileSystemId, std::move(capability), kWidevineKeySystem,
      false);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// On desktop Linux, given |cdm_base_path| that points to a folder containing
// the Widevine CDM and associated files, read the manifest included in that
// directory and create a CdmInfo. If that is successful, return the CdmInfo. If
// not, return nullptr.
std::unique_ptr<content::CdmInfo> CreateCdmInfoFromWidevineDirectory(
    const base::FilePath& cdm_base_path) {
  // Library should be inside a platform specific directory.
  auto cdm_library_path =
      media::GetPlatformSpecificDirectory(cdm_base_path)
          .Append(base::GetNativeLibraryName(kWidevineCdmLibraryName));
  if (!base::PathExists(cdm_library_path))
    return nullptr;

  // Manifest should be at the top level.
  auto manifest_path = cdm_base_path.Append(FILE_PATH_LITERAL("manifest.json"));
  base::Version version;
  content::CdmCapability capability;
  if (!ParseCdmManifestFromPath(manifest_path, &version, &capability))
    return nullptr;

  return CreateWidevineCdmInfo(version, cdm_library_path,
                               std::move(capability));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // (BUILDFLAG(BUNDLE_WIDEVINE_CDM) ||
        // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)) && (defined(OS_LINUX) ||
        // defined(OS_CHROMEOS))

#if BUILDFLAG(BUNDLE_WIDEVINE_CDM) && \
    (defined(OS_LINUX) || defined(OS_CHROMEOS))
// On Linux/ChromeOS we have to preload the CDM since it uses the zygote
// sandbox. On Windows and Mac, the bundled CDM is handled by the component
// updater.

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<content::CdmInfo> CreateCdmInfoForChromeOS(
    const base::FilePath& install_dir) {
  // On ChromeOS the Widevine CDM library is in the component directory and
  // does not have a manifest.
  // TODO(crbug.com/971433): Move Widevine CDM to a separate folder in the
  // component directory so that the manifest can be included.
  auto cdm_library_path =
      install_dir.Append(base::GetNativeLibraryName(kWidevineCdmLibraryName));
  if (!base::PathExists(cdm_library_path))
    return nullptr;

  // As there is no manifest, set |capability| as if it came from one. These
  // values must match the CDM that is being bundled with Chrome.
  content::CdmCapability capability;

  // Add the supported codecs as if they came from the component manifest.
  capability.video_codecs.push_back(media::VideoCodec::kCodecVP8);
  capability.video_codecs.push_back(media::VideoCodec::kCodecVP9);
  capability.video_codecs.push_back(media::VideoCodec::kCodecAV1);
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  capability.video_codecs.push_back(media::VideoCodec::kCodecH264);
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  // Both encryption schemes are supported on ChromeOS.
  capability.encryption_schemes.insert(media::EncryptionScheme::kCenc);
  capability.encryption_schemes.insert(media::EncryptionScheme::kCbcs);

  // Both temporary and persistent sessions are supported on ChromeOS.
  capability.session_types.insert(media::CdmSessionType::kTemporary);
  capability.session_types.insert(media::CdmSessionType::kPersistentLicense);

  return CreateWidevineCdmInfo(base::Version(WIDEVINE_CDM_VERSION_STRING),
                               cdm_library_path, std::move(capability));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// This code checks to see if the Widevine CDM was bundled with Chrome. If one
// can be found and looks valid, it returns the CdmInfo for the CDM. Otherwise
// it returns nullptr.
content::CdmInfo* GetBundledWidevine() {
  // We only want to do this on the first call, as if Widevine wasn't bundled
  // with Chrome (or it was deleted/removed) it won't be loaded into the zygote.
  static base::NoDestructor<std::unique_ptr<content::CdmInfo>> s_cdm_info(
      []() -> std::unique_ptr<content::CdmInfo> {
        base::FilePath install_dir;
        CHECK(base::PathService::Get(chrome::DIR_BUNDLED_WIDEVINE_CDM,
                                     &install_dir));

#if BUILDFLAG(IS_CHROMEOS_ASH)
        // On ChromeOS the Widevine CDM library is in the component directory
        // (returned above) and does not have a manifest.
        // TODO(crbug.com/971433): Move Widevine CDM to a separate folder in
        // the component directory so that the manifest can be included.
        return CreateCdmInfoForChromeOS(install_dir);
#else
        // On desktop Linux the MANIFEST is bundled with the CDM.
        return CreateCdmInfoFromWidevineDirectory(install_dir);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      }());
  return s_cdm_info->get();
}
#endif  // BUILDFLAG(BUNDLE_WIDEVINE_CDM) && (defined(OS_LINUX) ||
        // defined(OS_CHROMEOS))

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT) && \
    (defined(OS_LINUX) || defined(OS_CHROMEOS))
// This code checks to see if a component updated Widevine CDM can be found. If
// there is one and it looks valid, return the CdmInfo for that CDM. Otherwise
// return nullptr.
content::CdmInfo* GetComponentUpdatedWidevine() {
  // We only want to do this on the first call, as the component updater may run
  // and download a new version once Chrome has been running for a while. Since
  // the first returned version will be the one loaded into the zygote, we want
  // to return the same thing on subsequent calls.
  static base::NoDestructor<std::unique_ptr<content::CdmInfo>> s_cdm_info(
      []() -> std::unique_ptr<content::CdmInfo> {
        auto install_dir = GetLatestComponentUpdatedWidevineCdmDirectory();
        if (install_dir.empty())
          return nullptr;

        return CreateCdmInfoFromWidevineDirectory(install_dir);
      }());
  return s_cdm_info->get();
}
#endif  // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT) && (defined(OS_LINUX) ||
        // defined(OS_CHROMEOS))

}  // namespace

ChromeContentClient::ChromeContentClient() {
}

ChromeContentClient::~ChromeContentClient() {
}

#if BUILDFLAG(ENABLE_NACL)
void ChromeContentClient::SetNaClEntryFunctions(
    content::PepperPluginInfo::GetInterfaceFunc get_interface,
    content::PepperPluginInfo::PPP_InitializeModuleFunc initialize_module,
    content::PepperPluginInfo::PPP_ShutdownModuleFunc shutdown_module) {
  g_nacl_get_interface = get_interface;
  g_nacl_initialize_module = initialize_module;
  g_nacl_shutdown_module = shutdown_module;
}
#endif

#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_PDF)
void ChromeContentClient::SetPDFEntryFunctions(
    content::PepperPluginInfo::GetInterfaceFunc get_interface,
    content::PepperPluginInfo::PPP_InitializeModuleFunc initialize_module,
    content::PepperPluginInfo::PPP_ShutdownModuleFunc shutdown_module) {
  g_pdf_get_interface = get_interface;
  g_pdf_initialize_module = initialize_module;
  g_pdf_shutdown_module = shutdown_module;
}
#endif

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

#if BUILDFLAG(ENABLE_PLUGINS)
// static
content::PepperPluginInfo* ChromeContentClient::FindMostRecentPlugin(
    const std::vector<std::unique_ptr<content::PepperPluginInfo>>& plugins) {
  if (plugins.empty())
    return nullptr;

  using PluginSortKey = std::tuple<base::Version, bool>;

  std::map<PluginSortKey, content::PepperPluginInfo*> plugin_map;

  for (auto& plugin : plugins) {
    base::Version version(plugin->version);
    DCHECK(version.IsValid());
    plugin_map[PluginSortKey(version, plugin->is_external)] = plugin.get();
  }

  return plugin_map.rbegin()->second;
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

void ChromeContentClient::AddPepperPlugins(
    std::vector<content::PepperPluginInfo>* plugins) {
#if BUILDFLAG(ENABLE_PLUGINS)
  ComputeBuiltInPlugins(plugins);
#endif  // BUILDFLAG(ENABLE_PLUGINS)
}

void ChromeContentClient::AddContentDecryptionModules(
    std::vector<content::CdmInfo>* cdms,
    std::vector<media::CdmHostFilePath>* cdm_host_file_paths) {
  if (cdms) {
#if BUILDFLAG(ENABLE_WIDEVINE) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
    // The Widevine CDM on Linux needs to be registered (and loaded) before the
    // zygote is locked down. The CDM can be found from the version bundled with
    // Chrome (if BUNDLE_WIDEVINE_CDM = true) and/or the version downloaded by
    // the component updater (if ENABLE_WIDEVINE_CDM_COMPONENT = true). If two
    // versions exist, take the one with the higher version number.
    //
    // Note that the component updater will detect the bundled version, and if
    // there is no newer version available, select the bundled version. In this
    // case both versions will be the same and point to the same directory, so
    // it doesn't matter which one is loaded.
    content::CdmInfo* bundled_widevine = nullptr;
#if BUILDFLAG(BUNDLE_WIDEVINE_CDM)
    bundled_widevine = GetBundledWidevine();
#endif

    content::CdmInfo* updated_widevine = nullptr;
#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
    updated_widevine = GetComponentUpdatedWidevine();
#endif

    // If only a bundled version is available, or both are available and the
    // bundled version is not less than the updated version, register the
    // bundled version. If only the updated version is available, or both are
    // available and the updated version is greater, then register the updated
    // version. If neither are available, then nothing is registered.
    if (bundled_widevine &&
        (!updated_widevine ||
         bundled_widevine->version >= updated_widevine->version)) {
      VLOG(1) << "Registering bundled Widevine " << bundled_widevine->version;
      cdms->push_back(*bundled_widevine);
    } else if (updated_widevine) {
      VLOG(1) << "Registering component updated Widevine "
              << updated_widevine->version;
      cdms->push_back(*updated_widevine);
    } else {
      VLOG(1) << "Widevine enabled but no library found";
    }
#endif  // BUILDFLAG(ENABLE_WIDEVINE) && (defined(OS_LINUX) ||
        // defined(OS_CHROMEOS))

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
    // Register Clear Key CDM if specified in command line.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    base::FilePath clear_key_cdm_path =
        command_line->GetSwitchValuePath(switches::kClearKeyCdmPathForTesting);
    if (!clear_key_cdm_path.empty() && base::PathExists(clear_key_cdm_path)) {
      // TODO(crbug.com/764480): Remove these after we have a central place for
      // External Clear Key (ECK) related information.
      // Normal External Clear Key key system.
      const char kExternalClearKeyKeySystem[] = "org.chromium.externalclearkey";
      // A variant of ECK key system that has a different GUID.
      const char kExternalClearKeyDifferentGuidTestKeySystem[] =
          "org.chromium.externalclearkey.differentguid";

      // Supported codecs are hard-coded in ExternalClearKeyProperties.
      content::CdmCapability capability(
          {}, {media::EncryptionScheme::kCenc, media::EncryptionScheme::kCbcs},
          {media::CdmSessionType::kTemporary,
           media::CdmSessionType::kPersistentLicense});

      // Register kExternalClearKeyDifferentGuidTestKeySystem first separately.
      // Otherwise, it'll be treated as a sub-key-system of normal
      // kExternalClearKeyKeySystem. See MultipleCdmTypes test in
      // ECKEncryptedMediaTest.
      cdms->push_back(content::CdmInfo(
          media::kClearKeyCdmDisplayName, media::kClearKeyCdmDifferentGuid,
          base::Version("0.1.0.0"), clear_key_cdm_path,
          media::kClearKeyCdmFileSystemId, capability,
          kExternalClearKeyDifferentGuidTestKeySystem, false));

      cdms->push_back(
          content::CdmInfo(media::kClearKeyCdmDisplayName,
                           media::kClearKeyCdmGuid, base::Version("0.1.0.0"),
                           clear_key_cdm_path, media::kClearKeyCdmFileSystemId,
                           capability, kExternalClearKeyKeySystem, true));
    }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
  }

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  if (cdm_host_file_paths)
    AddCdmHostFilePaths(cdm_host_file_paths);
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
static const char* const kChromeStandardURLSchemes[] = {
    extensions::kExtensionScheme,
    chrome::kChromeNativeScheme,
    chrome::kChromeSearchScheme,
    dom_distiller::kDomDistillerScheme,
#if defined(OS_ANDROID)
    embedder_support::kAndroidAppScheme,
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    chrome::kCrosScheme,
#endif
};

void ChromeContentClient::AddAdditionalSchemes(Schemes* schemes) {
  for (auto* standard_scheme : kChromeStandardURLSchemes)
    schemes->standard_schemes.push_back(standard_scheme);

#if defined(OS_ANDROID)
  schemes->referrer_schemes.push_back(embedder_support::kAndroidAppScheme);
#endif

  schemes->savable_schemes.push_back(extensions::kExtensionScheme);
  schemes->savable_schemes.push_back(chrome::kChromeSearchScheme);
  schemes->savable_schemes.push_back(dom_distiller::kDomDistillerScheme);

  // chrome-search: resources shouldn't trigger insecure content warnings.
  schemes->secure_schemes.push_back(chrome::kChromeSearchScheme);

  // Treat as secure because communication with them is entirely in the browser,
  // so there is no danger of manipulation or eavesdropping on communication
  // with them by third parties.
  schemes->secure_schemes.push_back(extensions::kExtensionScheme);

  // chrome-native: is a scheme used for placeholder navigations that allow
  // UIs to be drawn with platform native widgets instead of HTML.  These pages
  // should be treated as empty documents that can commit synchronously.
  schemes->empty_document_schemes.push_back(chrome::kChromeNativeScheme);
  schemes->no_access_schemes.push_back(chrome::kChromeNativeScheme);
  schemes->secure_schemes.push_back(chrome::kChromeNativeScheme);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  schemes->service_worker_schemes.push_back(extensions::kExtensionScheme);

  // As far as Blink is concerned, they should be allowed to receive CORS
  // requests. At the Extensions layer, requests will actually be blocked unless
  // overridden by the web_accessible_resources manifest key.
  // TODO(kalman): See what happens with a service worker.
  schemes->cors_enabled_schemes.push_back(extensions::kExtensionScheme);

  schemes->csp_bypassing_schemes.push_back(extensions::kExtensionScheme);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  schemes->local_schemes.push_back(content::kExternalFileScheme);
#endif

#if defined(OS_ANDROID)
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

base::StringPiece ChromeContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ChromeContentClient::GetDataResourceBytes(
    int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

gfx::Image& ChromeContentClient::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

#if defined(OS_MAC)
base::FilePath ChromeContentClient::GetChildProcessPath(
    int child_flags,
    const base::FilePath& helpers_path) {
  std::string helper_name(chrome::kHelperProcessExecutableName);
  if (child_flags == chrome::kChildProcessHelperAlerts) {
    helper_name += " (Alerts)";
    return helpers_path.Append(helper_name + ".app")
        .Append("Contents")
        .Append("MacOS")
        .Append(helper_name);
  }
  NOTREACHED() << "Unsupported child process flags!";
  return {};
}
#endif  // OS_MAC

std::string ChromeContentClient::GetProcessTypeNameInEnglish(int type) {
#if BUILDFLAG(ENABLE_NACL)
  switch (type) {
    case PROCESS_TYPE_NACL_LOADER:
      return "Native Client module";
    case PROCESS_TYPE_NACL_BROKER:
      return "Native Client broker";
  }
#endif

  NOTREACHED() << "Unknown child process type!";
  return "Unknown";
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

#if defined(OS_ANDROID)
media::MediaDrmBridgeClient* ChromeContentClient::GetMediaDrmBridgeClient() {
  return new ChromeMediaDrmBridgeClient();
}
#endif  // OS_ANDROID

void ChromeContentClient::ExposeInterfacesToBrowser(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    mojo::BinderMap* binders) {
  binders->Add(
      base::BindRepeating(
          [](mojo::PendingReceiver<heap_profiling::mojom::ProfilingClient>
                 receiver) {
            static base::NoDestructor<heap_profiling::ProfilingClient>
                profiling_client;
            profiling_client->BindToInterface(std::move(receiver));
          }),
      io_task_runner);
}
