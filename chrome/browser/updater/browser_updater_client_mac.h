// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_MAC_H_
#define CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_MAC_H_

#include <string>

#include "base/callback_forward.h"
#include "base/mac/scoped_nsobject.h"
#include "base/sequence_checker.h"
#include "chrome/browser/updater/browser_updater_client.h"
#import "chrome/updater/app/server/mac/service_protocol.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"

@interface CRUUpdateClientOnDemandImpl : NSObject <CRUUpdateServicing>
- (instancetype)initWithScope:(updater::UpdaterScope)scope;
@end

class BrowserUpdaterClientMac : public BrowserUpdaterClient {
 public:
  explicit BrowserUpdaterClientMac(updater::UpdaterScope scope);
  explicit BrowserUpdaterClientMac(
      base::scoped_nsobject<CRUUpdateClientOnDemandImpl> client);

 private:
  friend class UpdateClientMacTest;

  ~BrowserUpdaterClientMac() override;

  SEQUENCE_CHECKER(sequence_checker_);

  // BrowserUpdaterClient.
  void BeginRegister(const std::string& version,
                     updater::UpdateService::Callback callback) override;
  void BeginRunPeriodicTasks(base::OnceClosure callback) override;
  void BeginUpdateCheck(
      updater::UpdateService::StateChangeCallback state_change,
      updater::UpdateService::Callback callback) override;
  void BeginGetUpdaterVersion(
      base::OnceCallback<void(const std::string&)> callback) override;

  base::scoped_nsobject<CRUUpdateClientOnDemandImpl> client_;
};

#endif  // CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_MAC_H_
