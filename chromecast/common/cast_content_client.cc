// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cast_content_client.h"

#include <stdint.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chromecast/base/cast_constants.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/base/version.h"
#include "chromecast/chromecast_buildflags.h"
#include "components/cast/common/constants.h"
#include "content/public/common/cdm_info.h"
#include "media/base/media_switches.h"
#include "media/cdm/cdm_type.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_ANDROID)
#include <optional>

#include "chromecast/common/media/cast_media_drm_bridge_client.h"
#include "components/cdm/common/android_cdm_registration.h"
#endif

#if !BUILDFLAG(IS_FUCHSIA)
#include "base/no_destructor.h"
#include "components/services/heap_profiling/public/cpp/profiling_client.h"  // nogncheck
#include "mojo/public/cpp/bindings/pending_receiver.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/cdm/cdm_paths.h"  // nogncheck
#endif

#if BUILDFLAG(BUNDLE_WIDEVINE_CDM) && BUILDFLAG(IS_LINUX)
#include "base/no_destructor.h"
#include "components/cdm/common/cdm_manifest.h"
#include "media/base/cdm_capability.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
// component updated CDM on all desktop platforms and remove this.
// This file is In SHARED_INTERMEDIATE_DIR.
#include "widevine_cdm_version.h"  // nogncheck
#endif

namespace chromecast {
namespace shell {

namespace {

#if BUILDFLAG(BUNDLE_WIDEVINE_CDM) && BUILDFLAG(IS_LINUX)
// Copied from chrome_content_client.cc
std::unique_ptr<content::CdmInfo> CreateWidevineCdmInfo(
    const base::Version& version,
    const base::FilePath& cdm_library_path,
    media::CdmCapability capability) {
  return std::make_unique<content::CdmInfo>(
      kWidevineKeySystem, content::CdmInfo::Robustness::kSoftwareSecure,
      std::move(capability), /*supports_sub_key_systems=*/false,
      kWidevineCdmDisplayName, kWidevineCdmType, version, cdm_library_path);
}

// On desktop Linux, given |cdm_base_path| that points to a folder containing
// the Widevine CDM and associated files, read the manifest included in that
// directory and create a CdmInfo. If that is successful, return the CdmInfo. If
// not, return nullptr.
// Copied from chrome_content_client.cc
// TODO(crbug.com/40746872): move the functions to a common file.
std::unique_ptr<content::CdmInfo> CreateCdmInfoFromWidevineDirectory(
    const base::FilePath& cdm_base_path) {
  // Library should be inside a platform specific directory.
  auto cdm_library_path =
      media::GetPlatformSpecificDirectory(cdm_base_path)
          .Append(base::GetNativeLibraryName(kWidevineCdmLibraryName));
  if (!base::PathExists(cdm_library_path)) {
    LOG(ERROR) << "cdm library path doesn't exist";
    return nullptr;
  }

  // Manifest should be at the top level.
  auto manifest_path = cdm_base_path.Append(FILE_PATH_LITERAL("manifest.json"));
  base::Version version;
  media::CdmCapability capability;
  if (!ParseCdmManifestFromPath(manifest_path, &version, &capability))
    return nullptr;

  return CreateWidevineCdmInfo(version, cdm_library_path,
                               std::move(capability));
}

// This code checks to see if the Widevine CDM was bundled with Chrome. If one
// can be found and looks valid, it returns the CdmInfo for the CDM. Otherwise
// it returns nullptr.
// Copied from chrome_content_client.cc
content::CdmInfo* GetBundledWidevine() {
  // We only want to do this on the first call, as if Widevine wasn't bundled
  // with Chrome (or it was deleted/removed) it won't be loaded into the zygote.
  static base::NoDestructor<std::unique_ptr<content::CdmInfo>> s_cdm_info(
      []() -> std::unique_ptr<content::CdmInfo> {
        base::FilePath install_dir;
        CHECK(base::PathService::Get(chromecast::DIR_BUNDLED_WIDEVINE_CDM,
                                     &install_dir));

        // On desktop Linux the MANIFEST is bundled with the CDM.
        return CreateCdmInfoFromWidevineDirectory(install_dir);
      }());
  return s_cdm_info->get();
}
#endif  // BUILDFLAG(BUNDLE_WIDEVINE_CDM) && BUILDFLAG(IS_LINUX)

}  // namespace

CastContentClient::~CastContentClient() {
}

void CastContentClient::SetActiveURL(const GURL& url, std::string top_origin) {
  if (url.is_empty() || url == last_active_url_)
    return;
  LOG(INFO) << "Active URL: " << url.possibly_invalid_spec() << " for origin '"
            << top_origin << "'";
  last_active_url_ = url;
}

void CastContentClient::AddAdditionalSchemes(Schemes* schemes) {
  schemes->standard_schemes.push_back(kChromeResourceScheme);
}

std::u16string CastContentClient::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

std::string_view CastContentClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* CastContentClient::GetDataResourceBytes(
    int resource_id) {
  // Chromecast loads localized resources for the home screen via this code
  // path. See crbug.com/643886 for details.
  return ui::ResourceBundle::GetSharedInstance().LoadLocalizedResourceBytes(
      resource_id);
}

std::string CastContentClient::GetDataResourceString(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      resource_id);
}

gfx::Image& CastContentClient::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

#if BUILDFLAG(IS_ANDROID)
::media::MediaDrmBridgeClient* CastContentClient::GetMediaDrmBridgeClient() {
  return new media::CastMediaDrmBridgeClient();
}
#endif  // BUILDFLAG(IS_ANDROID)

void CastContentClient::ExposeInterfacesToBrowser(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    mojo::BinderMap* binders) {
#if !BUILDFLAG(IS_FUCHSIA)
  binders->Add<heap_profiling::mojom::ProfilingClient>(
      base::BindRepeating(
          [](mojo::PendingReceiver<heap_profiling::mojom::ProfilingClient>
                 receiver) {
            static base::NoDestructor<heap_profiling::ProfilingClient>
                profiling_client;
            profiling_client->BindToInterface(std::move(receiver));
          }),
      io_task_runner);
#endif  // !BUILDFLAG(IS_FUCHSIA)
}

void CastContentClient::AddContentDecryptionModules(
    std::vector<content::CdmInfo>* cdms,
    std::vector<::media::CdmHostFilePath>* cdm_host_file_paths) {
  if (cdms) {
#if BUILDFLAG(BUNDLE_WIDEVINE_CDM) && BUILDFLAG(IS_LINUX)
    // The Widevine CDM on Linux needs to be registered (and loaded) before the
    // zygote is locked down. The CDM can be found from the version bundled with
    // Chrome (if BUNDLE_WIDEVINE_CDM = true).
    content::CdmInfo* bundled_widevine = GetBundledWidevine();

    if (bundled_widevine) {
      DVLOG(1) << "Registering bundled Widevine " << bundled_widevine->version;
      cdms->push_back(*bundled_widevine);
    } else {
      DVLOG(1) << "Widevine enabled but no library found";
    }
#elif BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_WIDEVINE)
    cdm::AddAndroidWidevineCdm(cdms);
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

#endif  // BUILDFLAG(BUNDLE_WIDEVINE_CDM) && BUILDFLAG(IS_LINUX)
  }
}

}  // namespace shell
}  // namespace chromecast
