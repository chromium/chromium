// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_GCM_DESKTOP_UTILS_H_
#define COMPONENTS_GCM_DRIVER_GCM_GCM_DESKTOP_UTILS_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "components/version_info/version_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom-forward.h"

class PrefService;
namespace base {
class FilePath;
}

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}

namespace gcm {

class GCMDriver;
class GCMClientFactory;

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
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_GCM_DESKTOP_UTILS_H_
