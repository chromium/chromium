// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_SCOPED_PROCESS_PROTECTOR_H_
#define CHROME_CHROME_CLEANER_TEST_SCOPED_PROCESS_PROTECTOR_H_

#include <windows.h>

#include "base/win/scoped_handle.h"

namespace chrome_cleaner {

// Used to prevent a process from being interacted with in any way except via
// taking ownership and resetting the dacl. Used by tests that want unkillable
// processes.
//
// This protection is defeated by any process that has SeDebugPrivilege (like
// a debugging process), since that allows the process to get the ALL_ACCESS
// handle.
class ScopedProcessProtector {
 public:
  // Constructs a ScopedProcessProtector that assigns an empty DACL to
  // |process_id|.
  explicit ScopedProcessProtector(uint32_t process_id);

  // Constructs a ScopedProcessProtector that updates the DACL for
  // |process_id|, adding DENY_ACCESS for all rights in |access_to_deny|.
  ScopedProcessProtector(uint32_t process_id, ACCESS_MASK access_to_deny);

  ~ScopedProcessProtector();

  bool Initialized() { return initialized_; }

  void Release();

 private:
  // Opens |process_id| and gets its current security descriptor. Returns true
  // on success.
  bool OpenProcess(uint32_t process_id);

  // Removes all access to the protected process. OpenProcess must be called
  // first.
  void DenyAllAccess();

  // Denies the rights in |access_to_deny| to the protected process.
  // OpenProcess must be called first.
  void DenyAccess(ACCESS_MASK access_to_deny);

  base::win::ScopedHandle process_handle_;
  bool initialized_ = false;
  PACL original_dacl_ = nullptr;
  PSECURITY_DESCRIPTOR original_descriptor_ = nullptr;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_SCOPED_PROCESS_PROTECTOR_H_
