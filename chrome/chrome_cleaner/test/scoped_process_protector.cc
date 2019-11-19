// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/scoped_process_protector.h"

#include <aclapi.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/string16.h"

namespace chrome_cleaner {

ScopedProcessProtector::ScopedProcessProtector(uint32_t process_id) {
  if (OpenProcess(process_id))
    DenyAllAccess();
}

ScopedProcessProtector::ScopedProcessProtector(uint32_t process_id,
                                               ACCESS_MASK access_to_deny) {
  if (OpenProcess(process_id))
    DenyAccess(access_to_deny);
}

ScopedProcessProtector::~ScopedProcessProtector() {
  Release();
}

void ScopedProcessProtector::Release() {
  if (initialized_) {
    if (SetSecurityInfo(process_handle_.Get(), SE_KERNEL_OBJECT,
                        DACL_SECURITY_INFORMATION, NULL, NULL, original_dacl_,
                        NULL) != ERROR_SUCCESS) {
      PLOG(ERROR) << "Failed to restore original DACL.";
    }
  }

  if (original_descriptor_) {
    ::LocalFree(original_descriptor_);
    original_dacl_ = nullptr;
    original_descriptor_ = nullptr;
  }
}

bool ScopedProcessProtector::OpenProcess(uint32_t process_id) {
  // Get an anything-goes handle to the process.
  process_handle_.Set(::OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id));
  if (!process_handle_.IsValid()) {
    PLOG(ERROR) << "Failed to open process: " << process_id;
    return false;
  }

  // Store its existing DACL for cleanup purposes. This API function is weird:
  // the pointer placed into |original_dacl_| is actually a pointer into a
  // the structure pointed to by |original_descriptor_|. To use this, one
  // stores both, but frees ONLY the structure stuffed into
  // |original_descriptor_|.
  if (GetSecurityInfo(process_handle_.Get(), SE_KERNEL_OBJECT,
                      DACL_SECURITY_INFORMATION, NULL, NULL, &original_dacl_,
                      NULL, &original_descriptor_) != ERROR_SUCCESS) {
    PLOG(ERROR) << "Failed to retreieve original DACL.";
    return false;
  }

  return true;
}

void ScopedProcessProtector::DenyAllAccess() {
  // Set a new empty DACL, effectively denying all things on the process
  // object.
  ACL dacl;
  if (!InitializeAcl(&dacl, sizeof(dacl), ACL_REVISION)) {
    PLOG(ERROR) << "Failed to initialize DACL";
    return;
  }
  if (SetSecurityInfo(process_handle_.Get(), SE_KERNEL_OBJECT,
                      DACL_SECURITY_INFORMATION, NULL, NULL, &dacl,
                      NULL) != ERROR_SUCCESS) {
    PLOG(ERROR) << "Failed to set new DACL.";
    return;
  }

  initialized_ = true;
}

void ScopedProcessProtector::DenyAccess(ACCESS_MASK access_to_deny) {
  // The name of the predefined EVERYONE group.
  static constexpr base::char16 kEveryoneGroup[] = STRING16_LITERAL("EVERYONE");

  // The Trustee parameter requires a non-const string.
  base::string16 trustee_name(kEveryoneGroup);

  EXPLICIT_ACCESS access = {};
  access.grfAccessPermissions = access_to_deny;
  access.grfAccessMode = DENY_ACCESS;
  access.grfInheritance = NO_INHERITANCE;
  access.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
  access.Trustee.ptstrName = &trustee_name[0];

  // Create a new DACL that merges |access| into the existing DACL.
  PACL new_dacl = nullptr;
  if (SetEntriesInAcl(1, &access, original_dacl_, &new_dacl) != ERROR_SUCCESS) {
    PLOG(ERROR) << "Failed to update DACL.";
    return;
  }
  base::ScopedClosureRunner free_new_dacl(
      base::BindOnce(base::IgnoreResult(&LocalFree), new_dacl));

  if (SetSecurityInfo(process_handle_.Get(), SE_KERNEL_OBJECT,
                      DACL_SECURITY_INFORMATION, NULL, NULL, new_dacl,
                      NULL) != ERROR_SUCCESS) {
    PLOG(ERROR) << "Failed to set new DACL.";
    return;
  }

  initialized_ = true;
}

}  // namespace chrome_cleaner
