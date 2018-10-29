// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_profile_service.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_driver_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(USE_GCM_FROM_PLATFORM)
#include "base/sequenced_task_runner.h"
#include "components/gcm_driver/gcm_driver_android.h"
#else
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/account_tracker.h"
#include "components/gcm_driver/gcm_account_tracker.h"
#include "components/gcm_driver/gcm_channel_status_syncer.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_desktop_utils.h"
#include "components/gcm_driver/gcm_driver_desktop.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#endif

namespace gcm {

#if !BUILDFLAG(USE_GCM_FROM_PLATFORM)
// Identity observer only has actual work to do when the user is actually signed
// in. It ensures that account tracker is taking
class GCMProfileService::IdentityObserver
    : public identity::IdentityManager::Observer {
 public:
  IdentityObserver(
      identity::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GCMDriver* driver);
  ~IdentityObserver() override;

  // identity::IdentityManager::Observer:
  void OnPrimaryAccountSet(const AccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const AccountInfo& previous_primary_account_info) override;

 private:
  void StartAccountTracker(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  GCMDriver* driver_;
  identity::IdentityManager* identity_manager_;
  std::unique_ptr<GCMAccountTracker> gcm_account_tracker_;

  // The account ID that this service is responsible for. Empty when the service
  // is not running.
  std::string account_id_;

  base::WeakPtrFactory<GCMProfileService::IdentityObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IdentityObserver);
};

GCMProfileService::IdentityObserver::IdentityObserver(
    identity::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GCMDriver* driver)
    : driver_(driver),
      identity_manager_(identity_manager),
      weak_ptr_factory_(this) {
  identity_manager_->AddObserver(this);

  OnPrimaryAccountSet(identity_manager_->GetPrimaryAccountInfo());
  StartAccountTracker(std::move(url_loader_factory));
}

GCMProfileService::IdentityObserver::~IdentityObserver() {
  if (gcm_account_tracker_)
    gcm_account_tracker_->Shutdown();
  identity_manager_->RemoveObserver(this);
}

void GCMProfileService::IdentityObserver::OnPrimaryAccountSet(
    const AccountInfo& primary_account_info) {
  // This might be called multiple times when the password changes.
  if (primary_account_info.account_id == account_id_)
    return;
  account_id_ = primary_account_info.account_id;

  // Still need to notify GCMDriver for UMA purpose.
  driver_->OnSignedIn();
}

void GCMProfileService::IdentityObserver::OnPrimaryAccountCleared(
    const AccountInfo& previous_primary_account_info) {
  account_id_.clear();

  // Still need to notify GCMDriver for UMA purpose.
  driver_->OnSignedOut();
}

void GCMProfileService::IdentityObserver::StartAccountTracker(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (gcm_account_tracker_)
    return;

  std::unique_ptr<AccountTracker> gaia_account_tracker(
      new AccountTracker(identity_manager_, std::move(url_loader_factory)));

  gcm_account_tracker_.reset(new GCMAccountTracker(
      std::move(gaia_account_tracker), identity_manager_, driver_));

  gcm_account_tracker_->Start();
}

#endif  // !BUILDFLAG(USE_GCM_FROM_PLATFORM)

// static
bool GCMProfileService::IsGCMEnabled(PrefService* prefs) {
#if BUILDFLAG(USE_GCM_FROM_PLATFORM)
  return true;
#else
  return prefs->GetBoolean(gcm::prefs::kGCMChannelStatus);
#endif  // BUILDFLAG(USE_GCM_FROM_PLATFORM)
}

#if BUILDFLAG(USE_GCM_FROM_PLATFORM)
GCMProfileService::GCMProfileService(
    base::FilePath path,
    scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner) {
  driver_.reset(new GCMDriverAndroid(path.Append(gcm_driver::kGCMStoreDirname),
                                     blocking_task_runner));
}
#else
GCMProfileService::GCMProfileService(
    PrefService* prefs,
    base::FilePath path,
    base::RepeatingCallback<
        void(base::WeakPtr<GCMProfileService>,
             network::mojom::ProxyResolvingSocketFactoryRequest)>
        get_socket_factory_callback,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    version_info::Channel channel,
    const std::string& product_category_for_subtypes,
    identity::IdentityManager* identity_manager,
    std::unique_ptr<GCMClientFactory> gcm_client_factory,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
    scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory) {
  driver_ = CreateGCMDriverDesktop(
      std::move(gcm_client_factory), prefs,
      path.Append(gcm_driver::kGCMStoreDirname),
      base::BindRepeating(get_socket_factory_callback,
                          weak_ptr_factory_.GetWeakPtr()),
      url_loader_factory, network_connection_tracker, channel,
      product_category_for_subtypes, ui_task_runner, io_task_runner,
      blocking_task_runner);

  identity_observer_.reset(new IdentityObserver(
      identity_manager_, url_loader_factory, driver_.get()));
}
#endif  // BUILDFLAG(USE_GCM_FROM_PLATFORM)

GCMProfileService::GCMProfileService() {}

GCMProfileService::~GCMProfileService() {}

void GCMProfileService::Shutdown() {
#if !BUILDFLAG(USE_GCM_FROM_PLATFORM)
  identity_observer_.reset();
#endif  // !BUILDFLAG(USE_GCM_FROM_PLATFORM)
  if (driver_) {
    driver_->Shutdown();
    driver_.reset();
  }
}

void GCMProfileService::SetDriverForTesting(std::unique_ptr<GCMDriver> driver) {
  driver_ = std::move(driver);

#if !BUILDFLAG(USE_GCM_FROM_PLATFORM)
  if (identity_observer_) {
    identity_observer_ = std::make_unique<IdentityObserver>(
        identity_manager_, url_loader_factory_, driver.get());
  }
#endif  // !BUILDFLAG(USE_GCM_FROM_PLATFORM)
}

}  // namespace gcm
