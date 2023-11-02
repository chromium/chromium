// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_EXTENSION_USER_CONTEXT_ENUMERATOR_H_
#define CHROME_CREDENTIAL_PROVIDER_EXTENSION_USER_CONTEXT_ENUMERATOR_H_

#include "base/win/windows_types.h"
#include "chrome/credential_provider/extension/task.h"

namespace credential_provider {
namespace extension {

// Provides utility method to enumerate associated GCPW users.
class UserContextEnumerator {
 public:
  // Returns an instance of UserContextEnumerator.
  static UserContextEnumerator* Get();

  // Performs the given |task| for every GCPW users on the device.
  HRESULT PerformTask(const std::string& task_name, Task& task);

 private:
  UserContextEnumerator();
  virtual ~UserContextEnumerator();

  // Returns the storage used for the instance pointer.
  static UserContextEnumerator** GetInstanceStorage();
};

}  // namespace extension

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_EXTENSION_USER_CONTEXT_ENUMERATOR_H_
