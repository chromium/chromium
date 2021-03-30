// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_UPDATE_SERVICE_PROXY_H_
#define CHROME_UPDATER_MAC_UPDATE_SERVICE_PROXY_H_

#import <Foundation/Foundation.h>

#include <string>

#include "base/callback_forward.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"

@class CRUUpdateServiceProxyImpl;

namespace base {
class SequencedTaskRunner;
class Version;
}  // namespace base

namespace update_client {
enum class Error;
}  // namespace update_client

namespace updater {

// All functions and callbacks must be called on the same sequence.
class UpdateServiceProxy : public UpdateService {
 public:
  explicit UpdateServiceProxy(UpdaterScope scope);

  // Overrides for UpdateService.
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) const override;
  void RegisterApp(
      const RegistrationRequest& request,
      base::OnceCallback<void(const RegistrationResponse&)> callback) override;
  void RunPeriodicTasks(base::OnceClosure callback) override;
  void UpdateAll(StateChangeCallback state_update, Callback callback) override;
  void Update(const std::string& app_id,
              Priority priority,
              StateChangeCallback state_update,
              Callback callback) override;
  void Uninitialize() override;

 private:
  ~UpdateServiceProxy() override;

  SEQUENCE_CHECKER(sequence_checker_);

  base::scoped_nsobject<CRUUpdateServiceProxyImpl> client_;
  scoped_refptr<base::SequencedTaskRunner> callback_runner_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_UPDATE_SERVICE_PROXY_H_
