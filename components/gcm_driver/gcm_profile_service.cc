// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_profile_service.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_driver_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(USE_GCM_FROM_PLATFORM)
#include "base/task/sequenced_task_runner.h"
#include "components/gcm_driver/gcm_driver_android.h"
#else
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/account_tracker.h"
#include "components/gcm_driver/gcm_account_tracker.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_desktop_utils.h"
#include "components/gcm_driver/gcm_driver_desktop.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#endif

namespace gcm {

#if BUILDFLAG(USE_GCM_FROM_PLATFORM)
GCMProfileService::GCMProfileService(
    base::FilePath path,
    scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner) {
  driver_ = std::make_unique<GCMDriverAndroid>(
      path.Append(gcm_driver::kGCMStoreDirname), blocking_task_runner);
}
#else
GCMProfileService::GCMProfileService(
    PrefService* prefs,
    base::FilePath path,
    base::RepeatingCallback<void(
        base::WeakPtr<GCMProfileService>,
        mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
        get_socket_factory_callback,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    version_info::Channel channel,
    const std::string& product_category_for_subtypes,
    signin::IdentityManager* identity_manager,
    std::unique_ptr<GCMClientFactory> gcm_client_factory,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
    scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)) {
  driver_ = CreateGCMDriverDesktop(
      std::move(gcm_client_factory), prefs,
      path.Append(gcm_driver::kGCMStoreDirname),
      base::BindRepeating(get_socket_factory_callback,
                          weak_ptr_factory_.GetWeakPtr()),
      url_loader_factory_, network_connection_tracker, channel,
      product_category_for_subtypes, ui_task_runner, io_task_runner,
      blocking_task_runner);

  if (identity_manager_) {
    gcm_account_tracker_ = std::make_unique<GCMAccountTracker>(
        std::make_unique<AccountTracker>(identity_manager_), identity_manager_,
        driver_.get());
    gcm_account_tracker_->Start();
  }
}
#endif  // BUILDFLAG(USE_GCM_FROM_PLATFORM)

GCMProfileService::GCMProfileService(std::unique_ptr<GCMDriver> gcm_driver)
    : driver_(std::move(gcm_driver)) {
}

GCMProfileService::~GCMProfileService() = default;

void GCMProfileService::Shutdown() {
#if !BUILDFLAG(USE_GCM_FROM_PLATFORM)
  if (gcm_account_tracker_) {
    gcm_account_tracker_->Shutdown();
    gcm_account_tracker_.reset();
  }
#endif  // !BUILDFLAG(USE_GCM_FROM_PLATFORM)
  if (driver_) {
    driver_->Shutdown();
    driver_.reset();
  }
}

}  // namespace gcm
