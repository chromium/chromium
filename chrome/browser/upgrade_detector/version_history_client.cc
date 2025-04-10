// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/version_history_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/channel_info.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "base/system/sys_info.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {

// Returns the name of the current channel, as ingested by the VersionHistory
// API.
std::string GetChannelString() {
  std::string channel =
      chrome::GetChannelName(chrome::WithExtendedStable(true));
  if (channel == "unknown") {
    return "stable";
  }
  if (!channel.empty()) {
    return channel;
  }
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // "" could mean Stable, LTC, or LTS. Find out which.
  std::string crosapi_channel_name;
  if (base::SysInfo::GetLsbReleaseValue(crosapi::kChromeOSReleaseTrack,
                                        &crosapi_channel_name)) {
    if (crosapi_channel_name == crosapi::kReleaseChannelLtc) {
      return "ltc";
    }
    if (crosapi_channel_name == crosapi::kReleaseChannelLts) {
      return "lts";
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return "stable";
}

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("version_history", R"(
        semantics {
          sender: "VersionHistory Client"
          description:
            "Queries the VersionHistory API to know how old the "
            "currently-running version of Chrome is. If it's older than a "
            "threshold configured by the admin, Chrome forces a relaunch (or "
            "ChromeOS forces a restart) to apply the update."
          trigger:
            "When Chrome detects that an update is available, if the "
            "RelaunchFastIfOutdated policy is set."
          data: "The version number of the currently-running Chrome/ChromeOS, "
            "the update channel name, and an identifier of the platform."
          destination: GOOGLE_OWNED_SERVICE
          user_data {
            type: NONE
          }
          internal {
             contacts {
               email: "cec-growth-eng@google.com"
             }
          }
          last_reviewed: "2025-03-06"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            RelaunchFastIfOutdated {
              RelaunchFastIfOutdated: 0
            }
          }
        })");

// Sends a GET to `url`, and calls `callback` with the response.
void FetchUrl(GURL url,
              base::OnceCallback<void(std::unique_ptr<network::SimpleURLLoader>,
                                      std::optional<std::string>)> callback) {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      g_browser_process->shared_url_loader_factory();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = std::move(url);
  request->load_flags = net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->priority = net::IDLE;

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(request), kTrafficAnnotation);
  url_loader->SetRetryOptions(
      /*max_retries=*/3,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);

  network::SimpleURLLoader* url_loader_raw = url_loader.get();
  // Move `url_loader` to the callback so the request can finish.
  url_loader_raw->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(std::move(callback), std::move(url_loader)),
      /*max_body_size=*/1024 * 1024);
}

// Parses and extract the most recent `serving.endTime` from `raw_data` JSON.
std::optional<base::Time> OnVersionReleasesFetched(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::optional<std::string> raw_data) {
  if (!raw_data) {
    return std::nullopt;
  }

  std::optional<base::Value::Dict> json = base::JSONReader::ReadDict(*raw_data);
  if (!json) {
    return std::nullopt;
  }

  const base::Value::List* releases = json->FindList("releases");
  if (!releases || releases->empty()) {
    return std::nullopt;
  }

  // The first element is always the most recent, due to the "order_by" query
  // parameter.
  const base::Value& most_recent_release = releases->front();
  if (!most_recent_release.is_dict()) {
    return std::nullopt;
  }

  const std::string* end_time =
      most_recent_release.GetDict().FindStringByDottedPath("serving.endTime");
  if (!end_time) {
    // Builds with no end-time are still serving, so they're not outdated.
    return std::nullopt;
  }

  base::Time time;
  if (!base::Time::FromUTCString(end_time->c_str(), &time)) {
    return std::nullopt;
  }

  return time;
}

}  // namespace

// Returns the URL to get version info of `version` on the current platform
// and channel. The response contains the single release with the most recent
// `serving.endTime`.
GURL GetVersionReleasesUrl(base::Version version) {
// CURRENT_PLATFORM is the platform name, as ingested by the VersionHistory API.
// Use #define instead of a constant, so it concatenates at compile-time.
#if BUILDFLAG(IS_WIN)

#if defined(ARCH_CPU_ARM64)
#define CURRENT_PLATFORM "win_arm64"
#elif defined(ARCH_CPU_X86_64)
#define CURRENT_PLATFORM "win64"
#else
#define CURRENT_PLATFORM "win"
#endif

#elif BUILDFLAG(IS_LINUX)

#define CURRENT_PLATFORM "linux"

#elif BUILDFLAG(IS_MAC)

#if defined(ARCH_CPU_ARM64)
#define CURRENT_PLATFORM "mac_arm64"
#else
#define CURRENT_PLATFORM "mac"
#endif

#elif BUILDFLAG(IS_CHROMEOS)

#define CURRENT_PLATFORM "chromeos"

#else

#error Unsupported platform

#endif  // BUILDFLAG(IS_WIN)

  return GURL(base::StringPrintf(
      "https://versionhistory.googleapis.com/v1/chrome/"
      "platforms/" CURRENT_PLATFORM
      "/channels/%s/versions/%s/releases/?order_by=endtime%%20desc&page_size=1",
      GetChannelString(), version.GetString()));
#undef CURRENT_PLATFORM
}

void GetLastServedDate(base::Version version, LastServedDateCallback callback) {
  FetchUrl(GetVersionReleasesUrl(std::move(version)),
           base::BindOnce(&OnVersionReleasesFetched).Then(std::move(callback)));
}
