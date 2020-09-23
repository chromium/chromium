// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_TEST_APP_UPDATE_CLIENT_H_
#define CHROME_UPDATER_TEST_TEST_APP_UPDATE_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "chrome/updater/test/test_app/constants.h"
#include "chrome/updater/update_service.h"

namespace base {
class SequencedTaskRunner;
}

namespace updater {

class UpdateClient : public base::RefCountedThreadSafe<UpdateClient> {
 public:
  using StatusCallback =
      base::RepeatingCallback<void(UpdateStatus status,
                                   int progress,
                                   bool rollback,
                                   const std::string& version,
                                   int64_t update_size,
                                   const base::string16& message)>;

  static scoped_refptr<UpdateClient> Create();

  UpdateClient();

  void Register(base::OnceCallback<void(int)> callback);
  void CheckForUpdate(StatusCallback callback);
  void HandleStatusUpdate(UpdateService::UpdateState update_state);
  void RegistrationCompleted(base::OnceCallback<void(int)> callback,
                             UpdateService::Result result);
  void UpdateCompleted(UpdateService::Result result);

 protected:
  friend class base::RefCountedThreadSafe<UpdateClient>;
  virtual ~UpdateClient();

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return callback_task_runner_;
  }

 private:
  bool CanCheckForUpdate();
  virtual void BeginRegister(const std::string& brand_code,
                             const std::string& tag,
                             const std::string& version,
                             UpdateService::Callback callback) = 0;
  virtual void BeginUpdateCheck(UpdateService::StateChangeCallback state_change,
                                UpdateService::Callback callback) = 0;
  virtual bool CanDialIPC() = 0;

  StatusCallback callback_;

  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_TEST_TEST_APP_UPDATE_CLIENT_H_
