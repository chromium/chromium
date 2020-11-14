// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_TEST_APP_UPDATE_CLIENT_WIN_H_
#define CHROME_UPDATER_TEST_TEST_APP_UPDATE_CLIENT_WIN_H_

#include "chrome/updater/test/test_app/update_client.h"

#include <windows.h>
#include <wrl/client.h>

struct IUpdater;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace updater {

class UpdateClientWin : public UpdateClient {
 public:
  UpdateClientWin();

 private:
  ~UpdateClientWin() override;

  // Overrides for UpdateClient.
  bool CanDialIPC() override;
  void BeginRegister(const std::string& brand_code,
                     const std::string& tag,
                     const std::string& version,
                     UpdateService::Callback callback) override;
  void BeginUpdateCheck(UpdateService::StateChangeCallback state_change,
                        UpdateService::Callback callback) override;

  void RegisterInternal(const std::string& brand_code,
                        const std::string& tag,
                        const std::string& version,
                        UpdateService::Callback callback);
  void UpdateCheckInternal(UpdateService::StateChangeCallback state_change,
                           UpdateService::Callback callback);

  // Task runner for update check & register.
  scoped_refptr<base::SingleThreadTaskRunner> com_task_runner_;

  Microsoft::WRL::ComPtr<IUpdater> updater_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_TEST_TEST_APP_UPDATE_CLIENT_WIN_H_
