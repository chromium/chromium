// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/util/scoped_token_privilege.h"

#include <shlobj.h>

#include <memory>

#include "base/containers/heap_array.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {
namespace {

// The privilege tested in ScopeTokenPrivilege tests below.
// Use SE_RESTORE_NAME as it is one of the many privileges that is available,
// but not enabled by default on processes running at high integrity.
constexpr wchar_t kTestedPrivilege[] = SE_RESTORE_NAME;

// Returns true if the current process' token has privilege |privilege_name|
// enabled.
bool CurrentProcessHasPrivilege(const wchar_t* privilege_name) {
  HANDLE temp_handle;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &temp_handle)) {
    ADD_FAILURE();
    return false;
  }

  base::win::ScopedHandle token(temp_handle);

  // First get the size of the buffer needed for |privileges| below.
  DWORD size;
  EXPECT_FALSE(
      ::GetTokenInformation(token.Get(), TokenPrivileges, nullptr, 0, &size));

  auto privileges_bytes = base::HeapArray<BYTE>::WithSize(size);
  TOKEN_PRIVILEGES* privileges =
      reinterpret_cast<TOKEN_PRIVILEGES*>(privileges_bytes.data());

  if (!::GetTokenInformation(token.Get(), TokenPrivileges, privileges, size,
                             &size)) {
    ADD_FAILURE();
    return false;
  }

  // There is no point getting a buffer to store more than |privilege_name|\0 as
  // anything longer will obviously not be equal to |privilege_name|.
  const DWORD desired_size = static_cast<DWORD>(wcslen(privilege_name));
  const DWORD buffer_size = desired_size + 1;
  auto name_buffer = base::HeapArray<wchar_t>::WithSize(buffer_size);
  for (int i = privileges->PrivilegeCount - 1; i >= 0; --i) {
    LUID_AND_ATTRIBUTES& luid_and_att = privileges->Privileges[i];
    size = buffer_size;
    ::LookupPrivilegeName(nullptr, &luid_and_att.Luid, name_buffer.data(),
                          &size);
    if (size == desired_size &&
        wcscmp(name_buffer.data(), privilege_name) == 0) {
      return luid_and_att.Attributes == SE_PRIVILEGE_ENABLED;
    }
  }
  return false;
}

}  // namespace

// Note: This test is only valid when run at high integrity (i.e. it will fail
// at medium integrity).
TEST(ScopedTokenPrivilegeTest, Basic) {
  ASSERT_FALSE(CurrentProcessHasPrivilege(kTestedPrivilege));

  if (!::IsUserAnAdmin()) {
    LOG(WARNING) << "Skipping SetupUtilTest.ScopedTokenPrivilegeBasic due to "
                    "not running as admin.";
    return;
  }

  {
    ScopedTokenPrivilege test_scoped_privilege(kTestedPrivilege);
    ASSERT_TRUE(test_scoped_privilege.is_enabled());
    ASSERT_TRUE(CurrentProcessHasPrivilege(kTestedPrivilege));
  }

  ASSERT_FALSE(CurrentProcessHasPrivilege(kTestedPrivilege));
}

// Note: This test is only valid when run at high integrity (i.e. it will fail
// at medium integrity).
TEST(ScopedTokenPrivilegeTest, AlreadyEnabled) {
  ASSERT_FALSE(CurrentProcessHasPrivilege(kTestedPrivilege));

  if (!::IsUserAnAdmin()) {
    LOG(WARNING) << "Skipping SetupUtilTest.ScopedTokenPrivilegeAlreadyEnabled "
                    "due to not running as admin.";
    return;
  }

  {
    ScopedTokenPrivilege test_scoped_privilege(kTestedPrivilege);
    ASSERT_TRUE(test_scoped_privilege.is_enabled());
    ASSERT_TRUE(CurrentProcessHasPrivilege(kTestedPrivilege));
    {
      ScopedTokenPrivilege dup_scoped_privilege(kTestedPrivilege);
      ASSERT_TRUE(dup_scoped_privilege.is_enabled());
      ASSERT_TRUE(CurrentProcessHasPrivilege(kTestedPrivilege));
    }
    ASSERT_TRUE(CurrentProcessHasPrivilege(kTestedPrivilege));
  }

  ASSERT_FALSE(CurrentProcessHasPrivilege(kTestedPrivilege));
}

}  // namespace installer
