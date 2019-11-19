// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cast_content_client.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chromecast/base/cast_constants.h"
#include "chromecast/base/version.h"
#include "chromecast/chromecast_buildflags.h"
#include "content/public/common/user_agent.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/url_util.h"

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
#include "extensions/common/constants.h"  // nogncheck
#endif

#if defined(OS_ANDROID)
#include "chromecast/common/media/cast_media_drm_bridge_client.h"
#endif

#if !defined(OS_FUCHSIA)
#include "base/no_destructor.h"
#include "components/services/heap_profiling/public/cpp/profiling_client.h"  // nogncheck
#include "mojo/public/cpp/bindings/pending_receiver.h"
#endif

namespace chromecast {
namespace shell {

namespace {

#if defined(OS_ANDROID)
std::string BuildAndroidOsInfo() {
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);

  std::string android_version_str;
  base::StringAppendF(
      &android_version_str, "%d.%d", os_major_version, os_minor_version);
  if (os_bugfix_version != 0)
    base::StringAppendF(&android_version_str, ".%d", os_bugfix_version);

  std::string android_info_str;
  // Append the build ID.
  std::string android_build_id = base::SysInfo::GetAndroidBuildID();
  if (android_build_id.size() > 0)
    android_info_str += "; Build/" + android_build_id;

  std::string os_info;
  base::StringAppendF(
      &os_info,
      "Android %s%s",
      android_version_str.c_str(),
      android_info_str.c_str());
  return os_info;
}
#endif

}  // namespace

std::string GetUserAgent() {
  std::string product = "Chrome/" PRODUCT_VERSION;
  std::string os_info;
  base::StringAppendF(
      &os_info,
      "%s%s",
#if defined(OS_ANDROID)
      "Linux; ",
      BuildAndroidOsInfo().c_str()
#elif BUILDFLAG(USE_ANDROID_USER_AGENT)
                      "Linux; ", "Android"
#else
      "X11; ",
      content::BuildOSCpuInfo(false /* include_android_build_number */).c_str()
#endif
      );
  return content::BuildUserAgentFromOSAndProduct(os_info, product) +
      " CrKey/" CAST_BUILD_REVISION;
}

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
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  schemes->standard_schemes.push_back(extensions::kExtensionScheme);
  // Treat as secure because we only load extension code written by us.
  schemes->secure_schemes.push_back(extensions::kExtensionScheme);
  schemes->service_worker_schemes.push_back(extensions::kExtensionScheme);
  schemes->csp_bypassing_schemes.push_back(extensions::kExtensionScheme);
#endif
}

base::string16 CastContentClient::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece CastContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) {
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

gfx::Image& CastContentClient::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

#if defined(OS_ANDROID)
::media::MediaDrmBridgeClient* CastContentClient::GetMediaDrmBridgeClient() {
  return new media::CastMediaDrmBridgeClient();
}
#endif  // OS_ANDROID

void CastContentClient::ExposeInterfacesToBrowser(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    mojo::BinderMap* binders) {
#if !defined(OS_FUCHSIA)
  binders->Add(
      base::BindRepeating(
          [](mojo::PendingReceiver<heap_profiling::mojom::ProfilingClient>
                 receiver) {
            static base::NoDestructor<heap_profiling::ProfilingClient>
                profiling_client;
            profiling_client->BindToInterface(std::move(receiver));
          }),
      io_task_runner);
#endif  // !defined(OS_FUCHSIA)
}

}  // namespace shell
}  // namespace chromecast
