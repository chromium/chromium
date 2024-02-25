// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/win/registry_settings_client.h"

#include "base/strings/string_number_conversions_win.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_reg_util_win.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/shlwapi.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace {

const std::wstring kTestKeyPath = L"SOFTWARE\\Chromium\\DeviceTrust\\Test";

// We will use this to test when registry stores QWORDS.
const int64_t kLargeNumberQword = 12147483647;

// Installs `value` in the given registry `path` and `hive`, under the key
// `name`. Returns false on errors.
bool InstallValue(const base::Value& value,
                  const std::wstring& name,
                  const int64_t qword_override = 0) {
  HKEY hive = HKEY_LOCAL_MACHINE;
  std::wstring path = kTestKeyPath;
  // KEY_ALL_ACCESS causes the ctor to create the key if it does not exist yet.
  base::win::RegKey key(hive, path.c_str(), KEY_ALL_ACCESS);

  // override header to write an int64_t directly.
  if (qword_override) {
    return key.WriteValue(name.c_str(), &qword_override,
                          static_cast<DWORD>(sizeof(qword_override)),
                          REG_QWORD) == ERROR_SUCCESS;
  }

  switch (value.type()) {
    case base::Value::Type::NONE:
      return key.WriteValue(name.c_str(), L"") == ERROR_SUCCESS;

    case base::Value::Type::BOOLEAN: {
      if (!value.is_bool())
        return false;
      return key.WriteValue(name.c_str(), value.GetBool() ? 1 : 0) ==
             ERROR_SUCCESS;
    }

    case base::Value::Type::DOUBLE: {
      std::wstring str_value = base::NumberToWString(value.GetDouble());
      return key.WriteValue(name.c_str(), str_value.c_str()) == ERROR_SUCCESS;
    }

    case base::Value::Type::INTEGER: {
      if (!value.is_int())
        return false;
      return key.WriteValue(name.c_str(), value.GetInt()) == ERROR_SUCCESS;
    }

    case base::Value::Type::STRING: {
      if (!value.is_string())
        return false;
      return key.WriteValue(
                 name.c_str(),
                 base::as_wcstr(base::UTF8ToUTF16(value.GetString()))) ==
             ERROR_SUCCESS;
    }

    default:
      return false;
  }
}

}  // namespace

namespace device_signals {

using GetSettingsSignalsCallback =
    base::OnceCallback<void(const std::vector<SettingsItem>&)>;

// Function for creating GetSettingsOptions.
GetSettingsOptions CreateOption(const std::string& name, bool get_value) {
  GetSettingsOptions option;
  option.path = base::SysWideToUTF8(kTestKeyPath);
  option.key = name;
  option.get_value = get_value;
  option.hive = RegistryHive::kHkeyLocalMachine;
  return option;
}

// Function for creating SettingsItem.
SettingsItem CreateSettingItem(const std::string& name,
                               PresenceValue value,
                               std::optional<std::string> setting_json_value) {
  SettingsItem item;
  item.path = base::SysWideToUTF8(kTestKeyPath);
  item.key = name;
  item.presence = value;
  item.hive = RegistryHive::kHkeyLocalMachine;
  item.setting_json_value = setting_json_value;

  return item;
}

class RegistrySettingsClientTest : public testing::Test {
 protected:
  void SetUp() override {
    registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE);

    // Install a DWORD.
    ASSERT_TRUE(InstallValue(base::Value(5), L"Test Key DWORD"));
    // Install a QWORD.
    ASSERT_TRUE(InstallValue(base::Value(), L"Test Key QWORD", 15));
    // Install a large QWORD (exceeds INTMAX).
    ASSERT_TRUE(InstallValue(base::Value(), L"Test Key LARGE QWORD",
                             kLargeNumberQword));
    // Install a REG_SZ.
    ASSERT_TRUE(
        InstallValue(base::Value("Place Holder STRING"), L"Test Key REG_SZ"));
    // Install a BOOL.
    ASSERT_TRUE(InstallValue(base::Value(true), L"Test Key BOOLEAN"));
    // Install a Empty Registry.
    ASSERT_TRUE(InstallValue(base::Value(), L"Test Key NONE"));
    // Install a DOUBLE.
    ASSERT_TRUE(InstallValue(base::Value(12.5), L"Test Key DOUBLE"));
  }

  base::test::TaskEnvironment task_environment_;
  base::test::TestFuture<const std::vector<::device_signals::SettingsItem>&>
      future_;
  RegistrySettingsClient client_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

// Tests an empty request to GetSettings. In this case we should have no setting
// items.
TEST_F(RegistrySettingsClientTest, GetSettings_EmptyOptions) {
  client_.GetSettings(std::vector<GetSettingsOptions>(), future_.GetCallback());
  EXPECT_EQ(std::vector<SettingsItem>(), future_.Get());
}

// Tests settings signal collections.
// - successful collection on various types
// - collection on unsupported reg types
// - registry not found
TEST_F(RegistrySettingsClientTest, GetSettings_AllCase) {
  std::vector<GetSettingsOptions> options;
  // Reading a DWORD.
  options.push_back(CreateOption("Test Key DWORD", false));
  options.push_back(CreateOption("Test Key DWORD", true));
  // Reading a QWORD.
  options.push_back(CreateOption("Test Key QWORD", false));
  options.push_back(CreateOption("Test Key QWORD", true));
  // Reading a QWORD larger than INTMAX.
  options.push_back(CreateOption("Test Key LARGE QWORD", true));
  // Reading a String.
  options.push_back(CreateOption("Test Key REG_SZ", false));
  options.push_back(CreateOption("Test Key REG_SZ", true));
  // Reading a Boolean.
  options.push_back(CreateOption("Test Key BOOLEAN", true));
  // Reading empty registry.
  options.push_back(CreateOption("Test Key NONE", true));
  // Reading non-existent registry.
  options.push_back(CreateOption("Test Key NON EXIST", true));
  // Reading a Double.
  options.push_back(CreateOption("Test Key DOUBLE", true));

  std::vector<SettingsItem> settings_items;
  settings_items.push_back(
      CreateSettingItem("Test Key DWORD", PresenceValue::kFound, std::nullopt));
  settings_items.push_back(
      CreateSettingItem("Test Key DWORD", PresenceValue::kFound, "5"));
  settings_items.push_back(
      CreateSettingItem("Test Key QWORD", PresenceValue::kFound, std::nullopt));
  settings_items.push_back(
      CreateSettingItem("Test Key QWORD", PresenceValue::kFound, "15"));
  settings_items.push_back(CreateSettingItem(
      "Test Key LARGE QWORD", PresenceValue::kFound, "12147483647"));
  settings_items.push_back(CreateSettingItem(
      "Test Key REG_SZ", PresenceValue::kFound, std::nullopt));
  settings_items.push_back(CreateSettingItem(
      "Test Key REG_SZ", PresenceValue::kFound, "\"Place Holder STRING\""));
  // settings client will read boolean registries as DWORD (and returns 0 or 1).
  settings_items.push_back(
      CreateSettingItem("Test Key BOOLEAN", PresenceValue::kFound, "1"));
  settings_items.push_back(
      CreateSettingItem("Test Key NONE", PresenceValue::kFound, "\"\""));
  // when key does not exist, client returns kNotFound and no setting value.
  settings_items.push_back(CreateSettingItem(
      "Test Key NON EXIST", PresenceValue::kNotFound, std::nullopt));
  // settings client will read unsupported type registries as REG_SZ (and
  // returns string value).
  settings_items.push_back(
      CreateSettingItem("Test Key DOUBLE", PresenceValue::kFound, "\"12.5\""));

  client_.GetSettings(options, future_.GetCallback());

  EXPECT_EQ(settings_items, future_.Get());
}

// Tests an request with no RegistryHive, which is required for
// registry settings client.
TEST_F(RegistrySettingsClientTest, GetSettings_InvalidHive) {
  GetSettingsOptions option;
  SettingsItem item;
  option.path = item.path = base::SysWideToUTF8(kTestKeyPath);
  option.key = item.key = "Test Inv Hive";
  option.get_value = true;
  option.hive = item.hive = std::nullopt;
  item.presence = PresenceValue::kNotFound;
  item.setting_json_value = std::nullopt;

  client_.GetSettings({option}, future_.GetCallback());

  EXPECT_EQ(std::vector<SettingsItem>({item}), future_.Get());
}

// Tests an request with no valid path.
TEST_F(RegistrySettingsClientTest, GetSettings_InvalidPath) {
  GetSettingsOptions option;
  SettingsItem item;
  option.path = item.path = "";
  option.key = item.key = "Test Inv Path";
  option.get_value = true;
  option.hive = item.hive = RegistryHive::kHkeyLocalMachine;
  item.presence = PresenceValue::kNotFound;
  item.setting_json_value = std::nullopt;

  client_.GetSettings({option}, future_.GetCallback());

  EXPECT_EQ(std::vector<SettingsItem>({item}), future_.Get());
}

// Tests an request to registry that stores QWORD/DWORD as REG_BINARY.
TEST_F(RegistrySettingsClientTest, GetSettings_BinaryValue) {
  // Extra setup for binary values.
  HKEY hive = HKEY_LOCAL_MACHINE;
  std::wstring path = kTestKeyPath;
  // KEY_ALL_ACCESS causes the ctor to create the key if it does not exist yet.
  base::win::RegKey key(hive, path.c_str(), KEY_ALL_ACCESS);
  DWORD small_bin = 31;
  key.WriteValue(L"Test Key BINARY32", &small_bin,
                 static_cast<DWORD>(sizeof(DWORD)), REG_BINARY);
  key.WriteValue(L"Test Key BINARY64", &kLargeNumberQword,
                 static_cast<DWORD>(sizeof(kLargeNumberQword)), REG_BINARY);

  std::vector<GetSettingsOptions> options;
  // Reading a DWORD.
  options.push_back(CreateOption("Test Key BINARY32", true));
  options.push_back(CreateOption("Test Key BINARY64", true));

  std::vector<SettingsItem> settings_items;
  settings_items.push_back(
      CreateSettingItem("Test Key BINARY32", PresenceValue::kFound, "31"));
  settings_items.push_back(CreateSettingItem(
      "Test Key BINARY64", PresenceValue::kFound, "12147483647"));

  client_.GetSettings(options, future_.GetCallback());

  EXPECT_EQ(std::vector<SettingsItem>(settings_items), future_.Get());
}

// Tests an request to registry that stores QWORD/DWORD as REG_BINARY.
TEST_F(RegistrySettingsClientTest, GetSettings_InvalidType) {
  // Extra setup for binary values.
  HKEY hive = HKEY_LOCAL_MACHINE;
  std::wstring path = kTestKeyPath;
  // KEY_ALL_ACCESS causes the ctor to create the key if it does not exist yet.
  base::win::RegKey key(hive, path.c_str(), KEY_ALL_ACCESS);
  int temp[2];
  key.WriteValue(L"Test Key Invalid Type", &temp,
                 static_cast<DWORD>(sizeof(temp)), REG_RESOURCE_LIST);

  std::vector<GetSettingsOptions> options;
  // Reading a DWORD.
  options.push_back(CreateOption("Test Key Invalid Type", true));

  std::vector<SettingsItem> settings_items;
  settings_items.push_back(CreateSettingItem(
      "Test Key Invalid Type", PresenceValue::kFound, std::nullopt));

  client_.GetSettings(options, future_.GetCallback());

  EXPECT_EQ(std::vector<SettingsItem>(settings_items), future_.Get());
}

}  // namespace device_signals
