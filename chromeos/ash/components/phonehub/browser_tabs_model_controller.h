// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_BROWSER_TABS_MODEL_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_BROWSER_TABS_MODEL_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model_provider.h"
#include "chromeos/ash/components/phonehub/mutable_phone_model.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

namespace ash {
namespace phonehub {

// This class sets a MutablePhoneModel by observing info provided by the
// BrowserTabsModelProvider.
class BrowserTabsModelController
    : public BrowserTabsModelProvider::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  BrowserTabsModelController(
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      BrowserTabsModelProvider* browser_tabs_model_provider,
      MutablePhoneModel* mutable_phone_model);
  ~BrowserTabsModelController() override;

 private:
  // BrowserTabsModelProvider::Observer:
  void OnBrowserTabsUpdated(
      bool is_sync_enabled,
      const std::vector<BrowserTabsModel::BrowserTabMetadata>&
          browser_tabs_metadata) override;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  void UpdateBrowserTabsModel();

  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;
  BrowserTabsModel cached_model_;
  raw_ptr<BrowserTabsModelProvider> browser_tabs_model_provider_;
  raw_ptr<MutablePhoneModel, DanglingUntriaged> mutable_phone_model_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_BROWSER_TABS_MODEL_CONTROLLER_H_
