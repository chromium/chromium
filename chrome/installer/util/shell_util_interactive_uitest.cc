// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/shell_util.h"

#include <shobjidl.h>
#include <stddef.h>
#include <wrl/client.h>

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/com_init_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::wstring GetCurrentDefault(
    IApplicationAssociationRegistration* registration,
    const wchar_t* query,
    ASSOCIATIONTYPE query_type,
    ASSOCIATIONLEVEL query_level) {
  base::win::ScopedCoMem<wchar_t> current_app;
  if (FAILED(registration->QueryCurrentDefault(query, query_type, query_level,
                                               &current_app))) {
    return std::wstring();
  }
  return current_app.get();
}

class ScopedCopyRegKey {
 public:
  ScopedCopyRegKey(const base::win::RegKey& from,
                   base::win::RegKey& to,
                   const wchar_t* key)
      : to_(to), key_(key) {
    base::win::RegKey exists_key(to_.Handle(), key_, KEY_READ | KEY_WRITE);
    if (exists_key.Valid()) {
      temp_key_name_ =
          base::StrCat({L"Temp-", base::ASCIIToWide(base::GenerateGUID())});
      LONG result = RegRenameKey(to_.Handle(), key_, temp_key_name_.c_str());
      if (result != ERROR_SUCCESS) {
        ADD_FAILURE() << "Registry Initial Rename Failed " << result;
        temp_key_name_.clear();
        return;
      }
    }

    base::win::RegKey orig_key(from.Handle(), key_, KEY_READ);
    base::win::RegKey dest_key(to_.Handle(), key_, KEY_WRITE);
    CopyRecursively(orig_key, dest_key);
    copied_ = true;
  }

  ~ScopedCopyRegKey() {
    if (copied_)
      to_.DeleteKey(key_);

    if (!temp_key_name_.empty()) {
      LONG result = RegRenameKey(to_.Handle(), temp_key_name_.c_str(), key_);
      if (result != ERROR_SUCCESS)
        ADD_FAILURE() << "Registry Restore Rename Failed " << result;
    }
  }

 private:
  static void CopyRecursively(const base::win::RegKey& from,
                              base::win::RegKey& to) {
    for (base::win::RegistryValueIterator value_iter(from.Handle(), L"");
         value_iter.Valid(); ++value_iter) {
      to.WriteValue(value_iter.Name(), value_iter.Value(),
                    value_iter.ValueSize(), value_iter.Type());
    }

    for (base::win::RegistryKeyIterator key_iter(from.Handle(), L"");
         key_iter.Valid(); ++key_iter) {
      const wchar_t* subkey_name = key_iter.Name();
      base::win::RegKey orig_key(from.Handle(), subkey_name, KEY_READ);
      base::win::RegKey dest_key(to.Handle(), subkey_name, KEY_WRITE);
      CopyRecursively(orig_key, dest_key);
    }
  }

  base::win::RegKey& to_;
  const wchar_t* key_;
  std::wstring temp_key_name_;
  bool copied_ = false;
};

}  // namespace

TEST(ShellUtilInteractiveTest, MakeChromeDefaultDirectly) {
  // Direct default setting is only supported on Win10 or above.
  if (base::win::GetVersion() < base::win::Version::WIN10)
    GTEST_SKIP();

  base::win::AssertComInitialized();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  // Saving the underlying HKCU\Software\Classes to forward reads from HKCR.
  base::win::RegKey original_hkcu_classes(
      HKEY_CURRENT_USER, L"Software\\Classes", KEY_READ | KEY_WRITE);
  registry_util::RegistryOverrideManager registry_overrides;
  ASSERT_NO_FATAL_FAILURE(
      registry_overrides.OverrideRegistry(HKEY_CURRENT_USER));

  Microsoft::WRL::ComPtr<IApplicationAssociationRegistration> registration;
  ASSERT_HRESULT_SUCCEEDED(
      ::CoCreateInstance(CLSID_ApplicationAssociationRegistration, nullptr,
                         CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&registration)));

  // IApplicationAssociationRegistration::SetAppAsDefault only works for
  // "MSEdgeHTM" for http, https, .htm, and .html on Win10+ and serves as a
  // convenient initial environment setup for this test.
  ASSERT_HRESULT_SUCCEEDED(
      registration->SetAppAsDefault(L"MSEdgeHTM", L"http", AT_URLPROTOCOL));
  ASSERT_HRESULT_SUCCEEDED(
      registration->SetAppAsDefault(L"MSEdgeHTM", L"https", AT_URLPROTOCOL));
  ASSERT_HRESULT_SUCCEEDED(
      registration->SetAppAsDefault(L"MSEdgeHTM", L".htm", AT_FILEEXTENSION));
  ASSERT_HRESULT_SUCCEEDED(
      registration->SetAppAsDefault(L"MSEdgeHTM", L".html", AT_FILEEXTENSION));

  ASSERT_EQ(L"MSEdgeHTM", GetCurrentDefault(registration.Get(), L"http",
                                            AT_URLPROTOCOL, AL_EFFECTIVE));
  ASSERT_EQ(L"MSEdgeHTM", GetCurrentDefault(registration.Get(), L"https",
                                            AT_URLPROTOCOL, AL_EFFECTIVE));
  ASSERT_EQ(L"MSEdgeHTM", GetCurrentDefault(registration.Get(), L".htm",
                                            AT_FILEEXTENSION, AL_EFFECTIVE));
  ASSERT_EQ(L"MSEdgeHTM", GetCurrentDefault(registration.Get(), L".html",
                                            AT_FILEEXTENSION, AL_EFFECTIVE));

  base::FilePath chrome_exe(temp_dir.GetPath().Append(installer::kChromeExe));
  ASSERT_TRUE(ShellUtil::MakeChromeDefaultDirectly(ShellUtil::CURRENT_USER,
                                                   chrome_exe, false));
  std::wstring prog_id = ShellUtil::GetCurrentProgIdForTesting(chrome_exe);

  // The following may query HKEY_CLASSES_ROOT for the progid, which merges
  // HKEY_CURERNT_USER and HKEY_LOCAL_MACHINE on the backend and bypasses the
  // RegistryOverrideManager redirect. This test will copy selected regkeys
  // to the underlying HKCU if necessary for correct functionality.
  base::win::RegKey redirected_hkcu_classes(HKEY_CURRENT_USER,
                                            L"Software\\Classes", KEY_READ);
  ScopedCopyRegKey copy_regkey(redirected_hkcu_classes, original_hkcu_classes,
                               prog_id.c_str());

  // If the expectations fail below, the default browser mechanism has changed
  // and will need to be reexamined.
  EXPECT_EQ(prog_id, GetCurrentDefault(registration.Get(), L"http",
                                       AT_URLPROTOCOL, AL_EFFECTIVE));
  EXPECT_EQ(prog_id, GetCurrentDefault(registration.Get(), L"https",
                                       AT_URLPROTOCOL, AL_EFFECTIVE));
  EXPECT_EQ(prog_id, GetCurrentDefault(registration.Get(), L".htm",
                                       AT_FILEEXTENSION, AL_EFFECTIVE));
  EXPECT_EQ(prog_id, GetCurrentDefault(registration.Get(), L".html",
                                       AT_FILEEXTENSION, AL_EFFECTIVE));
}
