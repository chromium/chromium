// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_TEST_SCOPED_SERVICE_HANDLE_H_
#define CHROME_CHROME_CLEANER_TEST_TEST_SCOPED_SERVICE_HANDLE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/os/scoped_service_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

// Offers the basic methods for using a service in the context of a test, such
// as installing, starting and closing the service that is defined by the
// ScopedServiceHandle class.
class TestScopedServiceHandle : public ScopedServiceHandle {
 public:
  ~TestScopedServiceHandle();

  ::testing::AssertionResult InstallService();
  ::testing::AssertionResult StartService();
  ::testing::AssertionResult StopAndDelete();
  void Close();

  const base::char16* service_name() const { return service_name_.c_str(); }

 private:
  base::string16 service_name_;
};

// Returns a random string that is not an existing service name. This is a
// best-effort check as a service with that name could be created before the
// function returns.
base::string16 RandomUnusedServiceNameForTesting();

// Tries to stop any copies of the test service executable that are running.
// Returns false if an executable remains running.
::testing::AssertionResult EnsureNoTestServicesRunning();

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_TEST_SCOPED_SERVICE_HANDLE_H_
