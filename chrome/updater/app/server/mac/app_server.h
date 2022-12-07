// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_MAC_APP_SERVER_H_
#define CHROME_UPDATER_APP_SERVER_MAC_APP_SERVER_H_

#include "base/memory/scoped_refptr.h"

#include <xpc/xpc.h>

#include "base/atomic_ref_count.h"
#include "base/callback_forward.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "chrome/updater/app/server/mac/service_delegate.h"
#include "chrome/updater/app/server/posix/app_server_posix.h"
#import "chrome/updater/configurator.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/update_service_impl.h"

namespace base {
struct RegistrationRequest;
}  // namespace base

namespace updater {

class UpdateServiceInternal;
class UpdateService;

class AppServerMac : public AppServerPosix {
 public:
  AppServerMac();

 protected:
  // Overrides of App.
  void Uninitialize() override;

 private:
  ~AppServerMac() override;

  // Overrides of AppServer.
  void ActiveDuty(scoped_refptr<UpdateService> update_service) override;
  void ActiveDutyInternal(
      scoped_refptr<UpdateServiceInternal> update_service_internal) override;
  bool SwapInNewVersion() override;
  bool MigrateLegacyUpdaters(
      base::RepeatingCallback<void(const RegistrationRequest&)>
          register_callback) override;
  void UninstallSelf() override;

  base::scoped_nsobject<CRUUpdateCheckServiceXPCDelegate>
      update_check_delegate_;
  base::scoped_nsobject<NSXPCListener> update_check_listener_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_MAC_APP_SERVER_H_
