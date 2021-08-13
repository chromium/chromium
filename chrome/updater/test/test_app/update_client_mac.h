// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_TEST_APP_UPDATE_CLIENT_MAC_H_
#define CHROME_UPDATER_TEST_TEST_APP_UPDATE_CLIENT_MAC_H_

#include "chrome/updater/test/test_app/update_client.h"

#include <string>

#include "base/mac/scoped_nsobject.h"
#include "chrome/updater/updater_scope.h"

@class CRUUpdateClientOnDemandImpl;

namespace updater {

class UpdateClientMac : public UpdateClient {
 public:
  explicit UpdateClientMac(UpdaterScope updater_scope);

 private:
  ~UpdateClientMac() override;

  // Overrides for UpdateClient.
  bool CanDialIPC() override;
  void BeginRegister(const std::string& brand_code,
                     const std::string& tag,
                     const std::string& version,
                     UpdateService::Callback callback) override;
  void BeginUpdateCheck(UpdateService::StateChangeCallback state_change,
                        UpdateService::Callback callback) override;

  base::scoped_nsobject<CRUUpdateClientOnDemandImpl> client_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_TEST_TEST_APP_UPDATE_CLIENT_MAC_H_
