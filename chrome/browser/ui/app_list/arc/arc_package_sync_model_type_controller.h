// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_PACKAGE_SYNC_MODEL_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_PACKAGE_SYNC_MODEL_TYPE_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/sync/driver/syncable_service_based_model_type_controller.h"

class Profile;

namespace syncer {
class SyncService;
}  // namespace syncer

// A DataTypeController for arc package sync datatypes, which enables or
// disables these types based on whether ArcAppInstance is ready.
class ArcPackageSyncModelTypeController
    : public syncer::SyncableServiceBasedModelTypeController,
      public ArcAppListPrefs::Observer,
      public arc::ArcSessionManager::Observer {
 public:
  // |dump_stack| is called when an unrecoverable error occurs.
  ArcPackageSyncModelTypeController(
      syncer::OnceModelTypeStoreFactory store_factory,
      base::WeakPtr<syncer::SyncableService> syncable_service,
      const base::RepeatingClosure& dump_stack,
      syncer::SyncService* sync_service,
      Profile* profile);
  ~ArcPackageSyncModelTypeController() override;

  // DataTypeController overrides.
  PreconditionState GetPreconditionState() const override;

  // ArcAppListPrefs::Observer overrides.
  void OnPackageListInitialRefreshed() override;

  // ArcSessionManager::Observer:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnArcInitialStart() override;

 private:
  syncer::SyncService* const sync_service_;
  Profile* const profile_;
  ArcAppListPrefs* const arc_prefs_;

  DISALLOW_COPY_AND_ASSIGN(ArcPackageSyncModelTypeController);
};
#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_PACKAGE_SYNC_MODEL_TYPE_CONTROLLER_H_
