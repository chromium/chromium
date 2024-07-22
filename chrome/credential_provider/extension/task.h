// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_EXTENSION_TASK_H_
#define CHROME_CREDENTIAL_PROVIDER_EXTENSION_TASK_H_

#include <vector>

#include "base/time/time.h"
#include "base/win/windows_types.h"
#include "chrome/credential_provider/extension/user_device_context.h"

namespace credential_provider {
namespace extension {

// Configuration the task needs to run on. A way to tell task manager on how
// to run the task.
struct Config {
  // Set a default execution period in case it isn't defined by individual
  // tasks.
  Config() : execution_period(base::Hours(1)) {}

  // The period that the task will be executed on.
  base::TimeDelta execution_period;
};

// An interface that can be implemented by individual GCPW tasks to be executed
// during periodic polling by GCPW extension service. Methods are called in the
// order they are defined. So, initially task runner gets the configuration of
// the task. Then it sets the context task will be running in. Lastly task is
// executed.
class Task {
 public:
  virtual ~Task() {}

  // ESA calls this function to get the execution config for the task. This
  // contains information about whether task is device level or
  // user level, failure action and etc.
  virtual Config GetConfig() = 0;

  // Based on the config of the task, UserDeviceContext contains identifiers for
  // the user and device. So the task can identify the users it is running on
  // behalf of.
  virtual HRESULT SetContext(const std::vector<UserDeviceContext>& c) = 0;

  // ESA calls execute function to perform the actual task.
  virtual HRESULT Execute() = 0;
};

}  // namespace extension
}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_EXTENSION_TASK_H_
