// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_desktop_utils.h"

#include <utility>

#include "base/command_line.h"
#include "base/task/sequenced_task_runner.h"
#include "components/fcm/engine/chrome_build_info.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_driver_desktop.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace gcm {

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
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    os_crypt_async::OSCryptAsync* os_crypt_async) {
  return std::unique_ptr<GCMDriver>(new GCMDriverDesktop(
      std::move(gcm_client_factory),
      fcm::GetChromeBuildInfo(channel, product_category_for_subtypes), prefs,
      store_path, get_socket_factory_callback, std::move(url_loader_factory),
      network_connection_tracker, ui_task_runner, io_task_runner,
      blocking_task_runner, os_crypt_async));
}

}  // namespace gcm
