// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/activity_impl.h"

#include <string>
#include <tuple>

#include "base/functional/function_ref.h"
#include "base/strings/strcat.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/user_info.h"
#include "chrome/updater/win/win_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace {

struct ReadActiveBitRetval {
  ReadActiveBitRetval() = default;
  ReadActiveBitRetval(DWORD result, bool active_bit)
      : read_result(result), active_bit_set(active_bit) {}
  ReadActiveBitRetval(const ReadActiveBitRetval&) = default;
  ReadActiveBitRetval& operator=(const ReadActiveBitRetval&) = default;

  DWORD read_result = ERROR_FILE_NOT_FOUND;
  bool active_bit_set = false;
};

using ReadActiveBitCallback =
    base::FunctionRef<ReadActiveBitRetval(base::win::RegKey&)>;
using WriteActiveBitCallback =
    base::FunctionRef<DWORD(base::win::RegKey&, bool)>;

struct ReadWriteCallbacks {
  ReadWriteCallbacks() = delete;
  ReadWriteCallbacks(ReadActiveBitCallback read, WriteActiveBitCallback write)
      : read_callback(read), write_callback(write) {}
  ReadWriteCallbacks(const ReadWriteCallbacks&) = default;
  ReadWriteCallbacks& operator=(const ReadWriteCallbacks&) = delete;

  ReadActiveBitCallback read_callback;
  WriteActiveBitCallback write_callback;
};

constexpr wchar_t kDidRun[] = L"dr";
constexpr char kAppId[] = "{6ACB7D4D-E5BA-48b0-85FE-A4051500A1BD}";
constexpr wchar_t kClientStateKeyPath[] =
    CLIENT_STATE_KEY L"{6ACB7D4D-E5BA-48b0-85FE-A4051500A1BD}";

DWORD WriteActiveBitAsString(base::win::RegKey& key, bool value) {
  return key.WriteValue(kDidRun, value ? L"1" : L"0");
}

DWORD WriteActiveBitAsDword(base::win::RegKey& key, bool value) {
  return key.WriteValue(kDidRun, value);
}

ReadActiveBitRetval ReadActiveBitAsString(base::win::RegKey& key) {
  std::wstring did_run_str(L"0");
  const DWORD result = key.ReadValue(kDidRun, &did_run_str);
  return ReadActiveBitRetval(result, did_run_str == L"1");
}

ReadActiveBitRetval ReadActiveBitAsDword(base::win::RegKey& key) {
  DWORD did_run = 0;
  const DWORD result = key.ReadValueDW(kDidRun, &did_run);
  return ReadActiveBitRetval(result, did_run);
}

}  // namespace

class ActivityWinTest : public ::testing::TestWithParam<
                            std::tuple<bool, bool, ReadWriteCallbacks>> {
 protected:
  void SetUp() override {
    std::wstring sid;
    ASSERT_HRESULT_SUCCEEDED(GetProcessUser(nullptr, nullptr, &sid));
    low_integrity_key_path_ =
        base::StrCat({USER_REG_VISTA_LOW_INTEGRITY_HKCU, L"\\", sid, L"\\",
                      kClientStateKeyPath});
    TearDown();

    CreateUserActiveBit(SetUserValue(), WriteActiveBitFn());
    CreateLowIntegrityUserActiveBit(SetLowUserValue(), WriteActiveBitFn());
  }

  void TearDown() override {
    base::win::RegKey(HKEY_CURRENT_USER, L"", Wow6432(DELETE))
        .DeleteKey(kClientStateKeyPath);
    base::win::RegKey(HKEY_CURRENT_USER, L"", Wow6432(DELETE))
        .DeleteKey(low_integrity_key_path_.c_str());
  }

  bool SetUserValue() const { return std::get<0>(GetParam()); }

  bool SetLowUserValue() const { return std::get<1>(GetParam()); }

  WriteActiveBitCallback WriteActiveBitFn() const {
    return std::get<2>(GetParam()).write_callback;
  }

  ReadActiveBitCallback ReadActiveBitFn() const {
    return std::get<2>(GetParam()).read_callback;
  }

  void CreateActiveBit(const std::wstring& key_name,
                       bool value,
                       WriteActiveBitCallback callback) const {
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, key_name.c_str(),
                                        Wow6432(KEY_SET_VALUE)));
    ASSERT_EQ(DWORD{ERROR_SUCCESS}, callback(key, value));
  }

  void DeleteActiveBit(const std::wstring& key_name) const {
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, key_name.c_str(),
                                      Wow6432(KEY_SET_VALUE)));
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteValue(kDidRun));
  }

  void CheckActiveBit(const std::wstring& key_name,
                      bool expected,
                      ReadActiveBitCallback callback) const {
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, key_name.c_str(),
                                      Wow6432(KEY_QUERY_VALUE)));

    const ReadActiveBitRetval retval = callback(key);
    ASSERT_EQ(DWORD{ERROR_SUCCESS}, retval.read_result);
    ASSERT_EQ(expected, retval.active_bit_set);
  }

  void CreateUserActiveBit(bool value, WriteActiveBitCallback callback) const {
    CreateActiveBit(kClientStateKeyPath, value, callback);
  }

  void DeleteUserActiveBit() const { DeleteActiveBit(kClientStateKeyPath); }

  void CheckUserActiveBit(bool expected, ReadActiveBitCallback callback) const {
    CheckActiveBit(kClientStateKeyPath, expected, callback);
  }

  void CreateLowIntegrityUserActiveBit(bool value,
                                       WriteActiveBitCallback callback) const {
    CreateActiveBit(low_integrity_key_path_, value, callback);
  }

  void DeleteLowIntegrityUserActiveBit() const {
    DeleteActiveBit(low_integrity_key_path_);
  }

  void CheckLowIntegrityUserActiveBit(bool expected,
                                      ReadActiveBitCallback callback) const {
    CheckActiveBit(low_integrity_key_path_, expected, callback);
  }

 private:
  std::wstring low_integrity_key_path_;
};

TEST_P(ActivityWinTest, GetActiveBit) {
  ASSERT_EQ(SetUserValue() || SetLowUserValue(),
            GetActiveBit(GetUpdaterScopeForTesting(), kAppId));

  CheckUserActiveBit(SetUserValue(), ReadActiveBitFn());
  CheckLowIntegrityUserActiveBit(SetLowUserValue(), ReadActiveBitFn());
}

TEST_P(ActivityWinTest, ClearActiveBit) {
  ClearActiveBit(GetUpdaterScopeForTesting(), kAppId);

  CheckUserActiveBit(false, &ReadActiveBitAsString);
  CheckLowIntegrityUserActiveBit(false, &ReadActiveBitAsString);
  ASSERT_FALSE(GetActiveBit(GetUpdaterScopeForTesting(), kAppId));
}

INSTANTIATE_TEST_SUITE_P(
    UpdaterScopeSetUserValueSetLowUserValueSetAsDword,
    ActivityWinTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Values(ReadWriteCallbacks(&ReadActiveBitAsString,
                                             &WriteActiveBitAsString),
                          ReadWriteCallbacks(&ReadActiveBitAsDword,
                                             &WriteActiveBitAsDword))));

}  // namespace updater
