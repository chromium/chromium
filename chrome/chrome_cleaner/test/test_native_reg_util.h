// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_TEST_NATIVE_REG_UTIL_H_
#define CHROME_CHROME_CLEANER_TEST_TEST_NATIVE_REG_UTIL_H_

#include <vector>

#include "base/strings/string16.h"
#include "base/win/registry.h"

namespace chrome_cleaner_sandbox {

// Creates a temporary registry key for tests to play under. Useful for tests
// using native APIs that will bypass advapi's registry redirection.
class ScopedTempRegistryKey {
 public:
  ScopedTempRegistryKey();
  ~ScopedTempRegistryKey();

  HANDLE Get();

  // Returne the relative registry path.
  const base::string16& Path() const;

  // Returns a fully qualified native registry path.
  const base::string16& FullyQualifiedPath() const;

 private:
  base::string16 key_path_;
  base::string16 fully_qualified_key_path_;
  base::win::RegKey reg_key_;
};

}  // namespace chrome_cleaner_sandbox

#endif  // CHROME_CHROME_CLEANER_TEST_TEST_NATIVE_REG_UTIL_H_
