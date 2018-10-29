// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/pepper_flash.h"
#include "chrome/common/secure_origin_whitelist.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/common_resources.h"
#include "components/crash/core/common/crash_key.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/net_log/chrome_net_log.h"
#include "components/services/heap_profiling/public/cpp/client.h"
#include "components/version_info/version_info.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_util.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/media_buildflags.h"
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/features/feature_util.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/common/pepper_plugin_info.h"
#include "flapper_version.h"  // nogncheck  In SHARED_INTERMEDIATE_DIR.
#include "ppapi/shared_impl/ppapi_permissions.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/cdm/cdm_paths.h"  // nogncheck
// Registers Widevine CDM if Widevine is enabled, the Widevine CDM is
// bundled and not a component. When the Widevine CDM is a component, it is
// registered in widevine_cdm_component_installer.cc.
#if BUILDFLAG(BUNDLE_WIDEVINE_CDM) && !BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
#define REGISTER_BUNDLED_WIDEVINE_CDM
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
// TODO(crbug.com/663554): Needed for WIDEVINE_CDM_VERSION_STRING. Support
// component updated CDM on all desktop platforms and remove this.
// This file is In SHARED_INTERMEDIATE_DIR.
#include "widevine_cdm_version.h"  // nogncheck
#endif
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

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
#endif  // BUILDFLAG(ENABLE_PDF)

content::PepperPluginInfo::GetInterfaceFunc g_pdf_get_interface;
content::PepperPluginInfo::PPP_InitializeModuleFunc g_pdf_initialize_module;
content::PepperPluginInfo::PPP_ShutdownModuleFunc g_pdf_shutdown_module;

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

  std::unique_ptr<base::DictionaryValue> manifest = base::DictionaryValue::From(
      base::JSONReader::Read(manifest_data, base::JSON_ALLOW_TRAILING_COMMAS));
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

#if defined(REGISTER_BUNDLED_WIDEVINE_CDM)
bool IsWidevineAvailable(base::FilePath* cdm_path,
                         content::CdmCapability* capability) {
  static enum {
    NOT_CHECKED,
    FOUND,
    NOT_FOUND,
  } widevine_cdm_file_check = NOT_CHECKED;

  if (base::PathService::Get(chrome::FILE_WIDEVINE_CDM, cdm_path)) {
    if (widevine_cdm_file_check == NOT_CHECKED)
      widevine_cdm_file_check = base::PathExists(*cdm_path) ? FOUND : NOT_FOUND;

    if (widevine_cdm_file_check == FOUND) {
      // Add the supported codecs as if they came from the component manifest.
      // This list must match the CDM that is being bundled with Chrome.
      capability->video_codecs.push_back(media::VideoCodec::kCodecVP8);
      capability->video_codecs.push_back(media::VideoCodec::kCodecVP9);
      // TODO(xhwang): Update this and tests after Widevine CDM supports VP9
      // profile 2.
      capability->supports_vp9_profile2 = false;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      capability->video_codecs.push_back(media::VideoCodec::kCodecH264);
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

      // Add the supported encryption schemes as if they came from the
      // component manifest. This list must match the CDM that is being
      // bundled with Chrome.
      capability->encryption_schemes.insert(media::EncryptionMode::kCenc);
      capability->encryption_schemes.insert(media::EncryptionMode::kCbcs);

      // Temporary session is always supported.
      capability->session_types.insert(media::CdmSessionType::kTemporary);
#if defined(OS_CHROMEOS)
      // TODO(crbug.com/767941): Push persistent-license support info here once
      // we check in a new CDM that supports it on Linux.
      capability->session_types.insert(
          media::CdmSessionType::kPersistentLicense);
#endif  // defined(OS_CHROMEOS)

      return true;
    }
  }

  return false;
}
#endif  // defined(REGISTER_BUNDLED_WIDEVINE_CDM)

std::string GetProduct() {
  return version_info::GetProductNameAndVersionForUserAgent();
}

}  // namespace

std::string GetUserAgent() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUserAgent)) {
    std::string ua = command_line->GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(ua))
      return ua;
    LOG(WARNING) << "Ignored invalid value for flag --" << switches::kUserAgent;
  }

  std::string product = GetProduct();
#if defined(OS_ANDROID)
  if (command_line->HasSwitch(switches::kUseMobileUserAgent))
    product += " Mobile";
#endif
  return content::BuildUserAgentFromProduct(product);
}

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

#if BUILDFLAG(ENABLE_PLUGINS)
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

  static crash_reporter::CrashKeyString<64> top_origin_key("top-origin");
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
#if defined(GOOGLE_CHROME_BUILD) && defined(FLAPPER_AVAILABLE)
    // Add a fake Flash plugin even though it doesn't actually exist - if a
    // web page requests it, it will be component-updated on-demand. There is
    // nothing that guarantees the component update will give us the
    // FLAPPER_VERSION_STRING version of Flash, but using this version seems
    // better than any other hardcoded alternative.
    plugins->push_back(
        CreatePepperFlashInfo(base::FilePath(ChromeContentClient::kNotPresent),
                              FLAPPER_VERSION_STRING, false));
#endif  // defined(GOOGLE_CHROME_BUILD) && defined(FLAPPER_AVAILABLE)
  }
#endif  // BUILDFLAG(ENABLE_PLUGINS)
}

void ChromeContentClient::AddContentDecryptionModules(
    std::vector<content::CdmInfo>* cdms,
    std::vector<media::CdmHostFilePath>* cdm_host_file_paths) {
  if (cdms) {
#if defined(REGISTER_BUNDLED_WIDEVINE_CDM)
    base::FilePath cdm_path;
    content::CdmCapability capability;
    if (IsWidevineAvailable(&cdm_path, &capability)) {
      const base::Version version(WIDEVINE_CDM_VERSION_STRING);
      DCHECK(version.IsValid());

      cdms->push_back(
          content::CdmInfo(kWidevineCdmDisplayName, kWidevineCdmGuid, version,
                           cdm_path, kWidevineCdmFileSystemId,
                           std::move(capability), kWidevineKeySystem, false));
    }
#endif  // defined(REGISTER_BUNDLED_WIDEVINE_CDM)

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
          {}, {media::EncryptionMode::kCenc, media::EncryptionMode::kCbcs},
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

  schemes->secure_origins = secure_origin_whitelist::GetWhitelist();

  // chrome-native: is a scheme used for placeholder navigations that allow
  // UIs to be drawn with platform native widgets instead of HTML.  These pages
  // should be treated as empty documents that can commit synchronously.
  schemes->empty_document_schemes.push_back(chrome::kChromeNativeScheme);
  schemes->no_access_schemes.push_back(chrome::kChromeNativeScheme);
  schemes->secure_schemes.push_back(chrome::kChromeNativeScheme);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extensions::feature_util::ExtensionServiceWorkersEnabled())
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

std::string ChromeContentClient::GetProduct() const {
  return ::GetProduct();
}

std::string ChromeContentClient::GetUserAgent() const {
  return ::GetUserAgent();
}

base::string16 ChromeContentClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ChromeContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ChromeContentClient::GetDataResourceBytes(
    int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

gfx::Image& ChromeContentClient::GetNativeImageNamed(int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

base::DictionaryValue ChromeContentClient::GetNetLogConstants() const {
  auto platform_dict = net_log::ChromeNetLog::GetPlatformConstants(
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
    const GURL& script_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return script_url.SchemeIs(extensions::kExtensionScheme);
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

void ChromeContentClient::OnServiceManagerConnected(
    content::ServiceManagerConnection* connection) {
  static base::LazyInstance<heap_profiling::Client>::Leaky profiling_client =
      LAZY_INSTANCE_INITIALIZER;

  std::unique_ptr<service_manager::BinderRegistry> registry(
      new service_manager::BinderRegistry);
  registry->AddInterface(
      base::BindRepeating(&heap_profiling::Client::BindToInterface,
                          base::Unretained(&profiling_client.Get())));
  connection->AddConnectionFilter(
      std::make_unique<content::SimpleConnectionFilter>(std::move(registry)));
}
