// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_desktop_utils.h"

#include <utility>

#include "base/command_line.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_driver_desktop.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace gcm {

namespace {

GCMClient::ChromePlatform GetPlatform() {
#if BUILDFLAG(IS_WIN)
  return GCMClient::PLATFORM_WIN;
#elif BUILDFLAG(IS_APPLE)
  return GCMClient::PLATFORM_MAC;
#elif BUILDFLAG(IS_IOS)
  return GCMClient::PLATFORM_IOS;
#elif BUILDFLAG(IS_ANDROID)
  return GCMClient::PLATFORM_ANDROID;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return GCMClient::PLATFORM_CROS;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return GCMClient::PLATFORM_LINUX;
#else
  // For all other platforms, return as LINUX.
  return GCMClient::PLATFORM_LINUX;
#endif
}

GCMClient::ChromeChannel GetChannel(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::UNKNOWN:
      return GCMClient::CHANNEL_UNKNOWN;
    case version_info::Channel::CANARY:
      return GCMClient::CHANNEL_CANARY;
    case version_info::Channel::DEV:
      return GCMClient::CHANNEL_DEV;
    case version_info::Channel::BETA:
      return GCMClient::CHANNEL_BETA;
    case version_info::Channel::STABLE:
      return GCMClient::CHANNEL_STABLE;
  }
  NOTREACHED_IN_MIGRATION();
  return GCMClient::CHANNEL_UNKNOWN;
}

std::string GetVersion() {
  return std::string(version_info::GetVersionNumber());
}

GCMClient::ChromeBuildInfo GetChromeBuildInfo(
    version_info::Channel channel,
    const std::string& product_category_for_subtypes) {
  GCMClient::ChromeBuildInfo chrome_build_info;
  chrome_build_info.platform = GetPlatform();
  chrome_build_info.channel = GetChannel(channel);
  chrome_build_info.version = GetVersion();
  chrome_build_info.product_category_for_subtypes =
      product_category_for_subtypes;
  return chrome_build_info;
}

}  // namespace

std::unique_ptr<GCMDriver> CreateGCMDriverDesktop(
    std::unique_ptr<GCMClientFactory> gcm_client_factory,
    PrefService* prefs,
    const base::FilePath& store_path,
    base::RepeatingCallback<void(
        mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
        get_socket_factory_callback,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    version_info::Channel channel,
    const std::string& product_category_for_subtypes,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner) {
  return std::unique_ptr<GCMDriver>(new GCMDriverDesktop(
      std::move(gcm_client_factory),
      GetChromeBuildInfo(channel, product_category_for_subtypes), prefs,
      store_path, get_socket_factory_callback, std::move(url_loader_factory),
      network_connection_tracker, ui_task_runner, io_task_runner,
      blocking_task_runner));
}

}  // namespace gcm
