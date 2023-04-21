// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/shell_util.h"

#include <shobjidl.h>
#include <stddef.h>
#include <wrl/client.h>

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ref.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/types/pass_key.h"
#include "base/uuid.h"
#include "base/win/com_init_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_co_mem.h"
#include "chrome/browser/chrome_for_testing/buildflags.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

// Copies one registry key to another location, preserving the destination and
// restoring it at destruction.
class ScopedCopyRegKey {
 public:
  // absl::optional requires a public constructor, so the constructor requires
  // a passkey to ensure callers use Create() to create this object.
  using ConstructorPassKey = base::PassKey<ScopedCopyRegKey>;

  // Copies |from|\|key| to |to|\|key|, preserving |to|\|key| and restoring it
  // at destruction. Returns nullopt upon failure of preserving |to|. |to| must
  // outlive this object.
  static absl::optional<ScopedCopyRegKey> Create(const base::win::RegKey& from,
                                                 base::win::RegKey& to,
                                                 const std::wstring& key) {
    absl::optional<ScopedCopyRegKey> copy_regkey;
    std::wstring temp_key_name;
    base::win::RegKey exists_key(to.Handle(), key.c_str(), KEY_READ);
    if (exists_key.Valid()) {
      temp_key_name = base::StrCat(
          {L"Temp-", base::ASCIIToWide(
                         base::Uuid::GenerateRandomV4().AsLowercaseString())});
      LONG result =
          RegRenameKey(to.Handle(), key.c_str(), temp_key_name.c_str());
      if (result != ERROR_SUCCESS) {
        ADD_FAILURE() << "Registry Initial Rename Failed " << result;
        return copy_regkey;
      }
    }

    base::win::RegKey orig_key(from.Handle(), key.c_str(), KEY_READ);
    base::win::RegKey dest_key(to.Handle(), key.c_str(), KEY_WRITE);
    CopyRecursively(orig_key, dest_key);
    copy_regkey.emplace(to, key, temp_key_name, ConstructorPassKey());
    return copy_regkey;
  }

  // |to| must outlive this object.
  ScopedCopyRegKey(base::win::RegKey& to,
                   const std::wstring& key,
                   const std::wstring& temp_key_name,
                   ConstructorPassKey passkey)
      : to_(to), key_(key), temp_key_name_(temp_key_name) {}

  ~ScopedCopyRegKey() {
    to_->DeleteKey(key_.c_str());

    if (!temp_key_name_.empty()) {
      LONG result =
          RegRenameKey(to_->Handle(), temp_key_name_.c_str(), key_.c_str());
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

  // This RegKey must outlive this class.
  const raw_ref<base::win::RegKey> to_;
  const std::wstring key_;
  const std::wstring temp_key_name_;
};

}  // namespace

TEST(ShellUtilInteractiveTest, MakeChromeDefaultDirectly) {
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
  //
  // However, some environments may not have MSEdgeHTM available, so the checks
  // below simply check that the current default isn't the prog id for testing,
  // which is sufficient for this test.
  ASSERT_HRESULT_SUCCEEDED(
      registration->SetAppAsDefault(L"MSEdgeHTM", L"http", AT_URLPROTOCOL));
  ASSERT_HRESULT_SUCCEEDED(
      registration->SetAppAsDefault(L"MSEdgeHTM", L"https", AT_URLPROTOCOL));
  ASSERT_HRESULT_SUCCEEDED(
      registration->SetAppAsDefault(L"MSEdgeHTM", L".htm", AT_FILEEXTENSION));
  ASSERT_HRESULT_SUCCEEDED(
      registration->SetAppAsDefault(L"MSEdgeHTM", L".html", AT_FILEEXTENSION));

  base::FilePath chrome_exe(temp_dir.GetPath().Append(installer::kChromeExe));
  std::wstring prog_id = ShellUtil::GetCurrentProgIdForTesting(chrome_exe);

  ASSERT_NE(prog_id, GetCurrentDefault(registration.Get(), L"http",
                                       AT_URLPROTOCOL, AL_EFFECTIVE));
  ASSERT_NE(prog_id, GetCurrentDefault(registration.Get(), L"https",
                                       AT_URLPROTOCOL, AL_EFFECTIVE));
  ASSERT_NE(prog_id, GetCurrentDefault(registration.Get(), L".htm",
                                       AT_FILEEXTENSION, AL_EFFECTIVE));
  ASSERT_NE(prog_id, GetCurrentDefault(registration.Get(), L".html",
                                       AT_FILEEXTENSION, AL_EFFECTIVE));

#if BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
  ASSERT_FALSE(ShellUtil::MakeChromeDefaultDirectly(ShellUtil::CURRENT_USER,
                                                    chrome_exe, false));
#else
  ASSERT_TRUE(ShellUtil::MakeChromeDefaultDirectly(ShellUtil::CURRENT_USER,
                                                   chrome_exe, false));
#endif

  // The following may query HKEY_CLASSES_ROOT for the progid, which merges
  // HKEY_CURERNT_USER and HKEY_LOCAL_MACHINE on the backend and bypasses the
  // RegistryOverrideManager redirect. This test will copy selected regkeys
  // to the underlying HKCU if necessary for correct functionality.
  base::win::RegKey redirected_hkcu_classes(HKEY_CURRENT_USER,
                                            L"Software\\Classes", KEY_READ);
  absl::optional<ScopedCopyRegKey> copy_regkey = ScopedCopyRegKey::Create(
      redirected_hkcu_classes, original_hkcu_classes, prog_id.c_str());
  ASSERT_TRUE(copy_regkey);

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
