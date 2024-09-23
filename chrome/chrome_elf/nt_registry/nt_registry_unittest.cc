// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/chrome_elf/nt_registry/nt_registry.h"

#include <windows.h>

#include <rpc.h>
#include <stddef.h>

#include "base/test/test_reg_util_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace {

//------------------------------------------------------------------------------
// WOW64 redirection tests
//
// - Only HKCU will be tested on the auto (try) bots.
//   HKLM will be kept separate (and manual) for local testing only.
//
// NOTE: Currently no real WOW64 context testing, as building x86 projects
//       during x64 builds is not currently supported for performance reasons.
// https://cs.chromium.org/chromium/src/build/toolchain/win/BUILD.gn?sq%3Dpackage:chromium&l=314
//------------------------------------------------------------------------------

// Utility function for the WOW64 redirection test suites.
// Note: Testing redirection through ADVAPI32 here as well, to get notice if
//       expected behaviour changes!
// If |redirected_path| == nullptr, no redirection is expected in any case.
void DoRedirectTest(nt::ROOT_KEY nt_root_key,
                    const wchar_t* path,
                    const wchar_t* redirected_path OPTIONAL) {
  HANDLE handle = INVALID_HANDLE_VALUE;
  HKEY key_handle = nullptr;
  constexpr ACCESS_MASK kAccess = KEY_WRITE | DELETE;
  const HKEY root_key =
      (nt_root_key == nt::HKCU) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;

  // Make sure clean before starting.
  nt::DeleteRegKey(nt_root_key, nt::NONE, path);
  if (redirected_path)
    nt::DeleteRegKey(nt_root_key, nt::NONE, redirected_path);

  //----------------------------------------------------------------------------
  // No redirection through ADVAPI32 on straight x86 or x64.
  ASSERT_EQ(ERROR_SUCCESS,
            RegCreateKeyExW(root_key, path, 0, nullptr, REG_OPTION_NON_VOLATILE,
                            kAccess, nullptr, &key_handle, nullptr));
  ASSERT_EQ(ERROR_SUCCESS, RegCloseKey(key_handle));
  ASSERT_TRUE(nt::OpenRegKey(nt_root_key, path, kAccess, &handle, nullptr));
  ASSERT_TRUE(nt::DeleteRegKey(handle));
  nt::CloseRegKey(handle);

#ifdef _WIN64
  //----------------------------------------------------------------------------
  // Try forcing WOW64 redirection on x64 through ADVAPI32.
  ASSERT_EQ(ERROR_SUCCESS,
            RegCreateKeyExW(root_key, path, 0, nullptr, REG_OPTION_NON_VOLATILE,
                            kAccess | KEY_WOW64_32KEY, nullptr, &key_handle,
                            nullptr));
  ASSERT_EQ(ERROR_SUCCESS, RegCloseKey(key_handle));
  // Check path:
  if (nt::OpenRegKey(nt_root_key, path, kAccess, &handle, nullptr)) {
    if (redirected_path)
      ADD_FAILURE();
    ASSERT_TRUE(nt::DeleteRegKey(handle));
    nt::CloseRegKey(handle);
  } else if (!redirected_path) {
    // Should have succeeded.
    ADD_FAILURE();
  }
  if (redirected_path) {
    // Check redirected path:
    if (nt::OpenRegKey(nt_root_key, redirected_path, kAccess, &handle,
                       nullptr)) {
      if (!redirected_path)
        ADD_FAILURE();
      ASSERT_TRUE(nt::DeleteRegKey(handle));
      nt::CloseRegKey(handle);
    } else {
      // Should have succeeded.
      ADD_FAILURE();
    }
  }

  //----------------------------------------------------------------------------
  // Try forcing WOW64 redirection on x64 through NTDLL.
  ASSERT_TRUE(
      nt::CreateRegKey(nt_root_key, path, kAccess | KEY_WOW64_32KEY, nullptr));
  // Check path:
  if (nt::OpenRegKey(nt_root_key, path, kAccess, &handle, nullptr)) {
    if (redirected_path)
      ADD_FAILURE();
    ASSERT_TRUE(nt::DeleteRegKey(handle));
    nt::CloseRegKey(handle);
  } else if (!redirected_path) {
    // Should have succeeded.
    ADD_FAILURE();
  }
  if (redirected_path) {
    // Check redirected path:
    if (nt::OpenRegKey(nt_root_key, redirected_path, kAccess, &handle,
                       nullptr)) {
      if (!redirected_path)
        ADD_FAILURE();
      ASSERT_TRUE(nt::DeleteRegKey(handle));
      nt::CloseRegKey(handle);
    } else {
      // Should have succeeded.
      ADD_FAILURE();
    }
  }
#endif  // _WIN64
}

// These test reg paths match |kClassesSubtree| in nt_registry.cc.
constexpr const wchar_t* kClassesRedirects[] = {
    L"SOFTWARE\\Classes\\CLSID\\chrome_testing",
    L"SOFTWARE\\Classes\\WOW6432Node\\CLSID\\chrome_testing",
    L"SOFTWARE\\Classes\\DirectShow\\chrome_testing",
    L"SOFTWARE\\Classes\\WOW6432Node\\DirectShow\\chrome_testing",
    L"SOFTWARE\\Classes\\Interface\\chrome_testing",
    L"SOFTWARE\\Classes\\WOW6432Node\\Interface\\chrome_testing",
    L"SOFTWARE\\Classes\\Media Type\\chrome_testing",
    L"SOFTWARE\\Classes\\WOW6432Node\\Media Type\\chrome_testing",
    L"SOFTWARE\\Classes\\MediaFoundation\\chrome_testing",
    L"SOFTWARE\\Classes\\WOW6432Node\\MediaFoundation\\chrome_testing"};

static_assert((_countof(kClassesRedirects) & 0x01) == 0,
              "Must have an even number of kClassesRedirects.");

// This test does NOT use NtRegistryTest class.  It requires Windows WOW64
// redirection to take place, which would not happen with a testing redirection
// layer.
TEST(NtRegistryTestRedirection, Wow64RedirectionHKCU) {
  // Using two elements for each loop.
  for (size_t index = 0; index < _countof(kClassesRedirects); index += 2) {
    DoRedirectTest(nt::HKCU, kClassesRedirects[index],
                   kClassesRedirects[index + 1]);
  }
}

// These test reg paths match |kHklmSoftwareSubtree| in nt_registry.cc.
constexpr const wchar_t* kHKLMNoRedirects[] = {
    L"SOFTWARE\\Classes\\chrome_testing",
    L"SOFTWARE\\Clients\\chrome_testing",
    L"SOFTWARE\\Microsoft\\COM3\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Cryptography\\Calais\\Current\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Cryptography\\Calais\\Readers\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Cryptography\\Services\\chrome_testing",
    L"SOFTWARE\\Microsoft\\CTF\\SystemShared\\chrome_testing",
    L"SOFTWARE\\Microsoft\\CTF\\TIP\\chrome_testing",
    L"SOFTWARE\\Microsoft\\DFS\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Driver Signing\\chrome_testing",
    L"SOFTWARE\\Microsoft\\EnterpriseCertificates\\chrome_testing",
    L"SOFTWARE\\Microsoft\\EventSystem\\chrome_testing",
    L"SOFTWARE\\Microsoft\\MSMQ\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Non-Driver Signing\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Notepad\\DefaultFonts\\chrome_testing",
    L"SOFTWARE\\Microsoft\\OLE\\chrome_testing",
    L"SOFTWARE\\Microsoft\\RAS\\chrome_testing",
    L"SOFTWARE\\Microsoft\\RPC\\chrome_testing",
    L"SOFTWARE\\Microsoft\\SOFTWARE\\Microsoft\\Shared "
    L"Tools\\MSInfo\\chrome_testing",
    L"SOFTWARE\\Microsoft\\SystemCertificates\\chrome_testing",
    L"SOFTWARE\\Microsoft\\TermServLicensing\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Transaction Server\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App "
    L"Paths\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Control "
    L"Panel\\Cursors\\Schemes\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers"
    L"\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\DriveIcons"
    L"\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\KindMap"
    L"\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Group "
    L"Policy\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers"
    L"\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Telephony\\Locations"
    L"\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows "
    L"NT\\CurrentVersion\\Console\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows "
    L"NT\\CurrentVersion\\FontDpi\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows "
    L"NT\\CurrentVersion\\FontLink\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows "
    L"NT\\CurrentVersion\\FontMapper\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows "
    L"NT\\CurrentVersion\\FontSubstitutes\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows "
    L"NT\\CurrentVersion\\Gre_initialize\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution "
    L"Options\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows "
    L"NT\\CurrentVersion\\LanguagePack\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\NetworkCards"
    L"\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows "
    L"NT\\CurrentVersion\\Perflib\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Ports\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Print\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList"
    L"\\chrome_testing",
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time "
    L"Zones\\chrome_testing",
    L"SOFTWARE\\Policies\\chrome_testing",
    L"SOFTWARE\\RegisteredApplications\\chrome_testing"};

// Run from administrator command prompt!
// Note: Disabled for automated testing (HKLM protection).  Local testing
// only.
//
// This test does NOT use NtRegistryTest class.  It requires Windows WOW64
// redirection to take place, which would not happen with a testing redirection
// layer.
TEST(NtRegistryTestRedirection, DISABLED_Wow64RedirectionHKLM) {
  // 1) SOFTWARE is redirected.
  DoRedirectTest(nt::HKLM, L"SOFTWARE\\chrome_testing",
                 L"SOFTWARE\\WOW6432Node\\chrome_testing");

  // 2) Except some subkeys are not.
  for (size_t index = 0; index < _countof(kHKLMNoRedirects); ++index) {
    DoRedirectTest(nt::HKLM, kHKLMNoRedirects[index], nullptr);
  }

  // 3) But then some Classes subkeys are redirected.
  // Using two elements for each loop.
  for (size_t index = 0; index < _countof(kClassesRedirects); index += 2) {
    DoRedirectTest(nt::HKLM, kClassesRedirects[index],
                   kClassesRedirects[index + 1]);
  }

  // 4) And just make sure other Classes subkeys are shared.
  DoRedirectTest(nt::HKLM, L"SOFTWARE\\Classes\\chrome_testing", nullptr);
}

TEST(NtRegistryTestMisc, SanitizeSubkeyPaths) {
  std::wstring new_path;
  EXPECT_TRUE(nt::SetTestingOverride(nt::HKCU, new_path));
  std::wstring sani_path = nt::GetTestingOverride(nt::HKCU);
  EXPECT_STREQ(L"", sani_path.c_str());

  new_path = L"boo";
  EXPECT_TRUE(nt::SetTestingOverride(nt::HKCU, new_path));
  sani_path = nt::GetTestingOverride(nt::HKCU);
  EXPECT_STREQ(L"boo", sani_path.c_str());

  new_path = L"\\boo";
  EXPECT_TRUE(nt::SetTestingOverride(nt::HKCU, new_path));
  sani_path = nt::GetTestingOverride(nt::HKCU);
  EXPECT_STREQ(L"boo", sani_path.c_str());

  new_path = L"boo\\";
  EXPECT_TRUE(nt::SetTestingOverride(nt::HKCU, new_path));
  sani_path = nt::GetTestingOverride(nt::HKCU);
  EXPECT_STREQ(L"boo", sani_path.c_str());

  new_path = L"\\\\\\";
  EXPECT_TRUE(nt::SetTestingOverride(nt::HKCU, new_path));
  sani_path = nt::GetTestingOverride(nt::HKCU);
  EXPECT_STREQ(L"", sani_path.c_str());

  new_path = L"boo\\\\\\ya";
  EXPECT_TRUE(nt::SetTestingOverride(nt::HKCU, new_path));
  sani_path = nt::GetTestingOverride(nt::HKCU);
  EXPECT_STREQ(L"boo\\ya", sani_path.c_str());

  new_path = L"\\\\\\boo\\ya\\\\";
  EXPECT_TRUE(nt::SetTestingOverride(nt::HKCU, new_path));
  sani_path = nt::GetTestingOverride(nt::HKCU);
  EXPECT_STREQ(L"boo\\ya", sani_path.c_str());

  // Be sure to leave the environment clean.
  new_path.clear();
  EXPECT_TRUE(nt::SetTestingOverride(nt::HKCU, new_path));
  sani_path = nt::GetTestingOverride(nt::HKCU);
  EXPECT_STREQ(L"", sani_path.c_str());
}

//------------------------------------------------------------------------------
// NtRegistryTest class
//
// Only use this class for tests that need testing registry redirection.
//------------------------------------------------------------------------------

class NtRegistryTest : public testing::Test {
 protected:
  void SetUp() override {
    std::wstring temp;
    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(HKEY_CURRENT_USER, &temp));
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKCU, temp));
    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE, &temp));
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKLM, temp));
  }

  void TearDown() override {
    std::wstring temp;
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKCU, temp));
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKLM, temp));
  }

 private:
  registry_util::RegistryOverrideManager override_manager_;
};

//------------------------------------------------------------------------------
// NT registry API tests
//------------------------------------------------------------------------------

TEST_F(NtRegistryTest, ApiDword) {
  HANDLE key_handle;
  const wchar_t* dword_val_name = L"DwordTestValue";
  DWORD dword_val = 1234;

  // Create a subkey to play under.
  ASSERT_TRUE(nt::CreateRegKey(nt::HKCU, L"NTRegistry\\dword", KEY_ALL_ACCESS,
                               &key_handle));
  ASSERT_NE(key_handle, INVALID_HANDLE_VALUE);
  ASSERT_NE(key_handle, nullptr);
  absl::Cleanup key_closer = [key_handle] { nt::CloseRegKey(key_handle); };

  DWORD get_dword = 0;
  EXPECT_FALSE(nt::QueryRegValueDWORD(key_handle, dword_val_name, &get_dword));

  // Set
  EXPECT_TRUE(nt::SetRegValueDWORD(key_handle, dword_val_name, dword_val));

  // Get
  EXPECT_TRUE(nt::QueryRegValueDWORD(key_handle, dword_val_name, &get_dword));
  EXPECT_EQ(get_dword, dword_val);

  // Clean up done by NtRegistryTest.
}

TEST_F(NtRegistryTest, ApiSz) {
  HANDLE key_handle;
  const wchar_t* sz_val_name = L"SzTestValue";
  std::wstring sz_val = L"blah de blah de blahhhhh.";
  const wchar_t* sz_val_name2 = L"SzTestValueEmpty";
  std::wstring sz_val2;
  const wchar_t* sz_val_name3 = L"SzTestValueMalformed";
  const wchar_t* sz_val_name4 = L"SzTestValueMalformed2";
  std::wstring sz_val3 = L"malformed";
  BYTE* sz_val3_byte = reinterpret_cast<BYTE*>(&sz_val3[0]);
  std::vector<BYTE> malform;
  for (size_t i = 0; i < (sz_val3.size() * sizeof(wchar_t)); i++)
    malform.push_back(sz_val3_byte[i]);
  const wchar_t* sz_val_name5 = L"SzTestValueSize0";

  // Create a subkey to play under.
  // ------------------------------
  ASSERT_TRUE(nt::CreateRegKey(nt::HKCU, L"NTRegistry\\sz", KEY_ALL_ACCESS,
                               &key_handle));
  ASSERT_NE(key_handle, INVALID_HANDLE_VALUE);
  ASSERT_NE(key_handle, nullptr);
  absl::Cleanup key_closer = [key_handle] { nt::CloseRegKey(key_handle); };

  std::wstring get_sz;
  EXPECT_FALSE(nt::QueryRegValueSZ(key_handle, sz_val_name, &get_sz));

  // Set
  // ------------------------------
  EXPECT_TRUE(nt::SetRegValueSZ(key_handle, sz_val_name, sz_val));
  EXPECT_TRUE(nt::SetRegValueSZ(key_handle, sz_val_name2, sz_val2));
  // No null terminator.
  EXPECT_TRUE(nt::SetRegKeyValue(key_handle, sz_val_name3, REG_SZ,
                                 malform.data(),
                                 static_cast<DWORD>(malform.size())));
  malform.push_back(0);
  // Single trailing 0 byte.
  EXPECT_TRUE(nt::SetRegKeyValue(key_handle, sz_val_name4, REG_SZ,
                                 malform.data(),
                                 static_cast<DWORD>(malform.size())));
  // Size 0 value.
  EXPECT_TRUE(nt::SetRegKeyValue(key_handle, sz_val_name5, REG_SZ, nullptr, 0));

  // Get
  // ------------------------------
  EXPECT_TRUE(nt::QueryRegValueSZ(key_handle, sz_val_name, &get_sz));
  EXPECT_EQ(get_sz, sz_val);
  EXPECT_TRUE(nt::QueryRegValueSZ(key_handle, sz_val_name2, &get_sz));
  EXPECT_EQ(get_sz, sz_val2);
  EXPECT_TRUE(nt::QueryRegValueSZ(key_handle, sz_val_name3, &get_sz));
  // Should be adjusted under the hood to equal sz_val3.
  EXPECT_EQ(get_sz, sz_val3);
  EXPECT_TRUE(nt::QueryRegValueSZ(key_handle, sz_val_name4, &get_sz));
  // Should be adjusted under the hood to equal sz_val3.
  EXPECT_EQ(get_sz, sz_val3);
  EXPECT_TRUE(nt::QueryRegValueSZ(key_handle, sz_val_name5, &get_sz));
  // Should be adjusted under the hood to an empty string.
  EXPECT_EQ(get_sz, sz_val2);

  // Clean up done by NtRegistryTest.
}

TEST_F(NtRegistryTest, ApiMultiSz) {
  HANDLE key_handle;
  std::vector<std::wstring> multisz_val_set;
  std::vector<std::wstring> multisz_val_get;

  // Create a subkey to play under.
  // ------------------------------
  ASSERT_TRUE(nt::CreateRegKey(nt::HKCU, L"NTRegistry\\multisz", KEY_ALL_ACCESS,
                               &key_handle));
  ASSERT_NE(key_handle, INVALID_HANDLE_VALUE);
  ASSERT_NE(key_handle, nullptr);
  absl::Cleanup key_closer = [key_handle] { nt::CloseRegKey(key_handle); };

  // Test 1 - Success
  // ------------------------------
  const wchar_t* multisz_val_name = L"SzmultiTestValue";
  std::wstring multi1 = L"one";
  std::wstring multi2 = L"two";
  std::wstring multi3 = L"three";

  multisz_val_set.push_back(multi1);
  multisz_val_set.push_back(multi2);
  multisz_val_set.push_back(multi3);
  EXPECT_TRUE(
      nt::SetRegValueMULTISZ(key_handle, multisz_val_name, multisz_val_set));

  EXPECT_TRUE(
      nt::QueryRegValueMULTISZ(key_handle, multisz_val_name, &multisz_val_get));
  EXPECT_EQ(multisz_val_get, multisz_val_set);
  multisz_val_set.clear();
  multisz_val_get.clear();

  // Test 2 - Bad value
  // ------------------------------
  const wchar_t* multisz_val_name2 = L"SzmultiTestValueBad";
  std::wstring multi_empty;

  multisz_val_set.push_back(multi_empty);
  EXPECT_TRUE(
      nt::SetRegValueMULTISZ(key_handle, multisz_val_name2, multisz_val_set));

  EXPECT_TRUE(nt::QueryRegValueMULTISZ(key_handle, multisz_val_name2,
                                       &multisz_val_get));
  EXPECT_EQ(multisz_val_get.size(), static_cast<DWORD>(0));
  multisz_val_set.clear();
  multisz_val_get.clear();

  // Test 3 - Malformed
  // ------------------------------
  std::wstring multisz_val3 = L"malformed";
  multisz_val_set.push_back(multisz_val3);
  BYTE* multisz_val3_byte = reinterpret_cast<BYTE*>(&multisz_val3[0]);
  std::vector<BYTE> malform;
  for (size_t i = 0; i < (multisz_val3.size() * sizeof(wchar_t)); i++)
    malform.push_back(multisz_val3_byte[i]);

  // 3.1: No null terminator.
  // ------------------------------
  const wchar_t* multisz_val_name3 = L"SzmultiTestValueMalformed";
  EXPECT_TRUE(nt::SetRegKeyValue(key_handle, multisz_val_name3, REG_MULTI_SZ,
                                 malform.data(),
                                 static_cast<DWORD>(malform.size())));
  EXPECT_TRUE(nt::QueryRegValueMULTISZ(key_handle, multisz_val_name3,
                                       &multisz_val_get));
  // Should be adjusted under the hood to equal multisz_val3.
  EXPECT_EQ(multisz_val_get, multisz_val_set);
  multisz_val_get.clear();

  // 3.2: Single trailing 0 byte.
  // ------------------------------
  const wchar_t* multisz_val_name4 = L"SzmultiTestValueMalformed2";
  malform.push_back(0);
  EXPECT_TRUE(nt::SetRegKeyValue(key_handle, multisz_val_name4, REG_MULTI_SZ,
                                 malform.data(),
                                 static_cast<DWORD>(malform.size())));
  EXPECT_TRUE(nt::QueryRegValueMULTISZ(key_handle, multisz_val_name4,
                                       &multisz_val_get));
  // Should be adjusted under the hood to equal multisz_val3.
  EXPECT_EQ(multisz_val_get, multisz_val_set);
  multisz_val_get.clear();

  // 3.3: Two trailing 0 bytes.
  // ------------------------------
  const wchar_t* multisz_val_name5 = L"SzmultiTestValueMalformed3";
  malform.push_back(0);
  EXPECT_TRUE(nt::SetRegKeyValue(key_handle, multisz_val_name5, REG_MULTI_SZ,
                                 malform.data(),
                                 static_cast<DWORD>(malform.size())));
  EXPECT_TRUE(nt::QueryRegValueMULTISZ(key_handle, multisz_val_name5,
                                       &multisz_val_get));
  // Should be adjusted under the hood to equal multisz_val3.
  EXPECT_EQ(multisz_val_get, multisz_val_set);
  multisz_val_get.clear();

  // 3.4: Three trailing 0 bytes.
  // ------------------------------
  const wchar_t* multisz_val_name6 = L"SzmultiTestValueMalformed4";
  malform.push_back(0);
  EXPECT_TRUE(nt::SetRegKeyValue(key_handle, multisz_val_name6, REG_MULTI_SZ,
                                 malform.data(),
                                 static_cast<DWORD>(malform.size())));
  EXPECT_TRUE(nt::QueryRegValueMULTISZ(key_handle, multisz_val_name6,
                                       &multisz_val_get));
  // Should be adjusted under the hood to equal multisz_val3.
  EXPECT_EQ(multisz_val_get, multisz_val_set);
  multisz_val_set.clear();
  multisz_val_get.clear();

  // Test 4 - Size zero
  // ------------------------------
  const wchar_t* multisz_val_name7 = L"SzmultiTestValueSize0";
  EXPECT_TRUE(nt::SetRegKeyValue(key_handle, multisz_val_name7, REG_MULTI_SZ,
                                 nullptr, 0));
  EXPECT_TRUE(nt::QueryRegValueMULTISZ(key_handle, multisz_val_name7,
                                       &multisz_val_get));
  // Should be empty.
  EXPECT_EQ(multisz_val_get, multisz_val_set);

  // Clean up done by NtRegistryTest.
}

TEST_F(NtRegistryTest, CreateRegKeyRecursion) {
  HANDLE key_handle;
  const wchar_t* sz_new_key_1 = L"test1\\new\\subkey";
  const wchar_t* sz_new_key_2 = L"test2\\new\\subkey\\blah\\";
  const wchar_t* sz_new_key_3 = L"\\test3\\new\\subkey\\\\blah2";

  // Tests for CreateRegKey recursion.
  ASSERT_TRUE(
      nt::CreateRegKey(nt::HKCU, sz_new_key_1, KEY_ALL_ACCESS, nullptr));
  EXPECT_TRUE(nt::OpenRegKey(nt::HKCU, sz_new_key_1, KEY_ALL_ACCESS,
                             &key_handle, nullptr));
  EXPECT_TRUE(nt::DeleteRegKey(key_handle));
  nt::CloseRegKey(key_handle);

  ASSERT_TRUE(
      nt::CreateRegKey(nt::HKCU, sz_new_key_2, KEY_ALL_ACCESS, nullptr));
  EXPECT_TRUE(nt::OpenRegKey(nt::HKCU, sz_new_key_2, KEY_ALL_ACCESS,
                             &key_handle, nullptr));
  EXPECT_TRUE(nt::DeleteRegKey(key_handle));
  nt::CloseRegKey(key_handle);

  ASSERT_TRUE(
      nt::CreateRegKey(nt::HKCU, sz_new_key_3, KEY_ALL_ACCESS, nullptr));
  EXPECT_TRUE(nt::OpenRegKey(nt::HKCU, L"test3\\new\\subkey\\blah2",
                             KEY_ALL_ACCESS, &key_handle, nullptr));
  EXPECT_TRUE(nt::DeleteRegKey(key_handle));
  nt::CloseRegKey(key_handle);

  // Subkey path can be null.
  ASSERT_TRUE(nt::CreateRegKey(nt::HKCU, nullptr, KEY_ALL_ACCESS, &key_handle));
  ASSERT_NE(key_handle, INVALID_HANDLE_VALUE);
  ASSERT_NE(key_handle, nullptr);
  nt::CloseRegKey(key_handle);

  // Clean up done by NtRegistryTest.
}

}  // namespace
