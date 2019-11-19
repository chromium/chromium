// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include <stdint.h>

#include <map>
#include <memory>
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
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/pepper_flash.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/common_resources.h"
#include "components/crash/core/common/crash_key.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/net_log/chrome_net_log.h"
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

#if defined(OS_LINUX)
#include <fcntl.h>
#include "chrome/common/component_flash_hint_file_linux.h"
#include "sandbox/linux/services/credentials.h"
#endif  // defined(OS_LINUX)

#if defined(OS_MACOSX)
#include "services/service_manager/sandbox/mac/nacl_loader.sb.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#include "components/nacl/common/nacl_process_type.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/common/pepper_plugin_info.h"
#include "flapper_version.h"  // nogncheck  In SHARED_INTERMEDIATE_DIR.
#include "ppapi/shared_impl/ppapi_permissions.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/cdm/cdm_paths.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_WIDEVINE) && defined(OS_LINUX)
#include "base/no_destructor.h"
#include "chrome/common/media/cdm_manifest.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
// TODO(crbug.com/663554): Needed for WIDEVINE_CDM_VERSION_STRING. Support
// component updated CDM on all desktop platforms and remove this.
// This file is In SHARED_INTERMEDIATE_DIR.
#include "widevine_cdm_version.h"  // nogncheck
#if !defined(OS_CHROMEOS)
#include "chrome/common/media/component_widevine_cdm_hint_file_linux.h"
#endif  // !defined(OS_CHROMEOS)
#endif  // BUILDFLAG(ENABLE_WIDEVINE) && defined(OS_LINUX)

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "chrome/common/media/cdm_host_file_path.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/common/media/chrome_media_drm_bridge_client.h"
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

// Creates a PepperPluginInfo for the specified plugin.
// |path| is the full path to the plugin.
// |version| is a string representation of the plugin version.
// |is_external| is whether the plugin is supplied external to Chrome e.g. a
//     system installation of Adobe Flash.
content::PepperPluginInfo CreatePepperFlashInfo(const base::FilePath& path,
                                                const std::string& version,
                                                bool is_external) {
  content::PepperPluginInfo plugin;

  plugin.is_out_of_process = true;
  plugin.name = content::kFlashPluginName;
  plugin.path = path;
  plugin.permissions = kPepperFlashPermissions;
  plugin.is_external = is_external;

  std::vector<std::string> flash_version_numbers = base::SplitString(
      version, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (flash_version_numbers.size() < 1)
    flash_version_numbers.push_back("11");
  if (flash_version_numbers.size() < 2)
    flash_version_numbers.push_back("2");
  if (flash_version_numbers.size() < 3)
    flash_version_numbers.push_back("999");
  if (flash_version_numbers.size() < 4)
    flash_version_numbers.push_back("999");
  // E.g., "Shockwave Flash 10.2 r154":
  plugin.description = plugin.name + " " + flash_version_numbers[0] + "." +
      flash_version_numbers[1] + " r" + flash_version_numbers[2];
  plugin.version = base::JoinString(flash_version_numbers, ".");
  content::WebPluginMimeType swf_mime_type(content::kFlashPluginSwfMimeType,
                                           content::kFlashPluginSwfExtension,
                                           content::kFlashPluginSwfDescription);
  plugin.mime_types.push_back(swf_mime_type);
  content::WebPluginMimeType spl_mime_type(content::kFlashPluginSplMimeType,
                                           content::kFlashPluginSplExtension,
                                           content::kFlashPluginSplDescription);
  plugin.mime_types.push_back(spl_mime_type);

  return plugin;
}

bool GetCommandLinePepperFlash(content::PepperPluginInfo* plugin) {
  const base::CommandLine::StringType flash_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kPpapiFlashPath);
  if (flash_path.empty())
    return false;

  // Also get the version from the command-line. Should be something like 11.2
  // or 11.2.123.45.
  std::string flash_version =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPpapiFlashVersion);

  *plugin = CreatePepperFlashInfo(base::FilePath(flash_path), flash_version,
                                  true);
  return true;
}

// Check if flash player exists on disk, and if so, populate a PepperPluginInfo
// structure. Returns false if the flash player found is not compatible with the
// system (architecture, OS, versions, etc.).
bool TryCreatePepperFlashInfo(const base::FilePath& flash_filename,
                              content::PepperPluginInfo* plugin) {
  if (!base::PathExists(flash_filename))
    return false;

  base::FilePath manifest_path(
      flash_filename.DirName().Append(FILE_PATH_LITERAL("manifest.json")));

  std::string manifest_data;
  if (!base::ReadFileToString(manifest_path, &manifest_data))
    return false;

  std::unique_ptr<base::DictionaryValue> manifest =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(
          manifest_data, base::JSON_ALLOW_TRAILING_COMMAS));
  if (!manifest)
    return false;

  base::Version version;
  if (!CheckPepperFlashManifest(*manifest, &version)) {
    LOG(ERROR) << "Browser not compatible with given flash manifest.";
    return false;
  }

  *plugin = CreatePepperFlashInfo(flash_filename, version.GetString(), true);
  return true;
}

#if defined(OS_CHROMEOS)
bool GetComponentUpdatedPepperFlash(content::PepperPluginInfo* plugin) {
  base::FilePath flash_filename;
  if (!base::PathService::Get(chrome::FILE_CHROME_OS_COMPONENT_FLASH,
                              &flash_filename)) {
    return false;
  }

  // Chrome OS mounts a disk image containing component updated flash player, at
  // boot time, if and only if a component update is present.
  if (!base::PathExists(flash_filename))
    return false;

  return TryCreatePepperFlashInfo(flash_filename, plugin);
}
#elif defined(OS_LINUX)
// This method is used on Linux only because of architectural differences in how
// it loads the component updated flash plugin, and not because the other
// platforms do not support component updated flash. On other platforms, the
// component updater sends an IPC message to all threads, at undefined points in
// time, with the URL of the component updated flash. Because the linux zygote
// thread has no access to the file system after it warms up, it must preload
// the component updated flash.
bool GetComponentUpdatedPepperFlash(content::PepperPluginInfo* plugin) {
#if defined(FLAPPER_AVAILABLE)
  if (component_flash_hint_file::DoesHintFileExist()) {
    base::FilePath flash_path;
    std::string version;
    if (component_flash_hint_file::VerifyAndReturnFlashLocation(&flash_path,
                                                                &version)) {
      // Test if the file can be mapped as executable. If the user's home
      // directory is mounted noexec, the component flash plugin will not load.
      // By testing for this, Chrome can fallback to the bundled flash plugin.
      if (!component_flash_hint_file::TestExecutableMapping(flash_path)) {
        LOG(WARNING) << "The component updated flash plugin could not be "
                        "mapped as executable. Attempting to fallback to the "
                        "bundled or system plugin.";
        return false;
      }
      *plugin = CreatePepperFlashInfo(flash_path, version, false);
      return true;
    }
    LOG(ERROR)
        << "Failed to locate and load the component updated flash plugin.";
  }
#endif  // defined(FLAPPER_AVAILABLE)
  return false;
}
#endif  // defined(OS_CHROMEOS)

bool GetSystemPepperFlash(content::PepperPluginInfo* plugin) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Do not try and find System Pepper Flash if there is a specific path on
  // the commmand-line.
  if (command_line->HasSwitch(switches::kPpapiFlashPath))
    return false;

  base::FilePath flash_filename;
  if (!base::PathService::Get(chrome::FILE_PEPPER_FLASH_SYSTEM_PLUGIN,
                              &flash_filename))
    return false;

  return TryCreatePepperFlashInfo(flash_filename, plugin);
}
#endif  //  BUILDFLAG(ENABLE_PLUGINS)

#if (BUILDFLAG(BUNDLE_WIDEVINE_CDM) ||            \
     BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)) && \
    defined(OS_LINUX)
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

#if !defined(OS_CHROMEOS)
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
#endif  // !defined(OS_CHROMEOS)
#endif  // (BUILDFLAG(BUNDLE_WIDEVINE_CDM) ||
        // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)) && defined(OS_LINUX)

#if BUILDFLAG(BUNDLE_WIDEVINE_CDM) && defined(OS_LINUX)
// On Linux/ChromeOS we have to preload the CDM since it uses the zygote
// sandbox. On Windows and Mac, the bundled CDM is handled by the component
// updater.

#if defined(OS_CHROMEOS)
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
  capability.supports_vp9_profile2 = true;
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
#endif  // defined(OS_CHROMEOS)

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

#if defined(OS_CHROMEOS)
        // On ChromeOS the Widevine CDM library is in the component directory
        // (returned above) and does not have a manifest.
        // TODO(crbug.com/971433): Move Widevine CDM to a separate folder in
        // the component directory so that the manifest can be included.
        return CreateCdmInfoForChromeOS(install_dir);
#else
        // On desktop Linux the MANIFEST is bundled with the CDM.
        return CreateCdmInfoFromWidevineDirectory(install_dir);
#endif  // defined(OS_CHROMEOS)
      }());
  return s_cdm_info->get();
}
#endif  // BUILDFLAG(BUNDLE_WIDEVINE_CDM) && defined(OS_LINUX)

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT) && defined(OS_LINUX)
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
#endif  // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT) && defined(OS_LINUX)

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

  // If flash is disabled, do not try to add any flash plugin.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool disable_bundled_flash =
      command_line->HasSwitch(switches::kDisableBundledPpapiFlash);

  std::vector<std::unique_ptr<content::PepperPluginInfo>> flash_versions;

// Get component updated flash for desktop Linux and Chrome OS.
#if defined(OS_LINUX)
  // Depending on the sandbox configuration, the file system
  // is not always available. If it is not available, do not try and load any
  // flash plugin. The flash player, if any, preloaded before the sandbox
  // initialization will continue to be used.
  if (!sandbox::Credentials::HasFileSystemAccess())
    return;

  auto component_flash = std::make_unique<content::PepperPluginInfo>();
  if (!disable_bundled_flash &&
      GetComponentUpdatedPepperFlash(component_flash.get()))
    flash_versions.push_back(std::move(component_flash));
#endif  // defined(OS_LINUX)

  auto command_line_flash = std::make_unique<content::PepperPluginInfo>();
  if (GetCommandLinePepperFlash(command_line_flash.get()))
    flash_versions.push_back(std::move(command_line_flash));

  auto system_flash = std::make_unique<content::PepperPluginInfo>();
  if (GetSystemPepperFlash(system_flash.get()))
    flash_versions.push_back(std::move(system_flash));

  // This function will return only the most recent version of the flash plugin.
  content::PepperPluginInfo* max_flash = FindMostRecentPlugin(flash_versions);
  if (max_flash) {
    plugins->push_back(*max_flash);
  } else if (!disable_bundled_flash) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(FLAPPER_AVAILABLE)
    // Add a fake Flash plugin even though it doesn't actually exist - if a
    // web page requests it, it will be component-updated on-demand. There is
    // nothing that guarantees the component update will give us the
    // FLAPPER_VERSION_STRING version of Flash, but using this version seems
    // better than any other hardcoded alternative.
    plugins->push_back(
        CreatePepperFlashInfo(base::FilePath(ChromeContentClient::kNotPresent),
                              FLAPPER_VERSION_STRING, false));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(FLAPPER_AVAILABLE)
  }
#endif  // BUILDFLAG(ENABLE_PLUGINS)
}

void ChromeContentClient::AddContentDecryptionModules(
    std::vector<content::CdmInfo>* cdms,
    std::vector<media::CdmHostFilePath>* cdm_host_file_paths) {
  if (cdms) {
#if BUILDFLAG(ENABLE_WIDEVINE) && defined(OS_LINUX)
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
#endif  // BUILDFLAG(ENABLE_WIDEVINE) && defined(OS_LINUX)

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
           media::CdmSessionType::kPersistentLicense,
           media::CdmSessionType::kPersistentUsageRecord},
          {});

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
#if defined(OS_CHROMEOS)
    chrome::kCrosScheme,
#endif
};

void ChromeContentClient::AddAdditionalSchemes(Schemes* schemes) {
  for (auto* standard_scheme : kChromeStandardURLSchemes)
    schemes->standard_schemes.push_back(standard_scheme);

#if defined(OS_ANDROID)
  schemes->referrer_schemes.push_back(chrome::kAndroidAppScheme);
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

#if defined(OS_CHROMEOS)
  schemes->local_schemes.push_back(content::kExternalFileScheme);
#endif

#if defined(OS_ANDROID)
  schemes->local_schemes.push_back(url::kContentScheme);
#endif
}

base::string16 ChromeContentClient::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

base::string16 ChromeContentClient::GetLocalizedString(
    int message_id,
    const base::string16& replacement) {
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

base::DictionaryValue ChromeContentClient::GetNetLogConstants() {
  auto platform_dict = net_log::GetPlatformConstantsForNetLog(
      base::CommandLine::ForCurrentProcess()->GetCommandLineString(),
      chrome::GetChannelName());
  if (platform_dict)
    return std::move(*platform_dict);
  else
    return base::DictionaryValue();
}

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

bool ChromeContentClient::AllowScriptExtensionForServiceWorker(
    const url::Origin& script_origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return script_origin.scheme() == extensions::kExtensionScheme;
#else
  return false;
#endif
}

blink::OriginTrialPolicy* ChromeContentClient::GetOriginTrialPolicy() {
  // Prevent initialization race (see crbug.com/721144). There may be a
  // race when the policy is needed for worker startup (which happens on a
  // separate worker thread).
  base::AutoLock auto_lock(origin_trial_policy_lock_);
  if (!origin_trial_policy_)
    origin_trial_policy_ = std::make_unique<ChromeOriginTrialPolicy>();
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
