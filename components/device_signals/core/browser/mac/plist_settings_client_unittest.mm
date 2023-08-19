// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/mac/plist_settings_client.h"

#import <Foundation/Foundation.h>

#include <string>

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/device_signals/test/test_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_signals {

class PlistSettingsClientTest : public testing::Test {
 protected:
  GetSettingsOptions CreateOption(const std::string& key_path, bool value) {
    GetSettingsOptions option;
    option.path = test_file_path_.value();
    option.key = key_path;
    option.get_value = value;
    return option;
  }

  SettingsItem CreateSettingItem(const std::string& key_path,
                                 PresenceValue value,
                                 const std::string& setting_value) {
    SettingsItem item;
    if (!setting_value.empty())
      item.setting_json_value = setting_value;
    return FinishSettingItemSetup(item, key_path, value);
  }

  SettingsItem CreateSettingItem(const std::string& key_path,
                                 PresenceValue value,
                                 const int setting_value) {
    SettingsItem item;
    item.setting_json_value = base::StringPrintf("%d", setting_value);
    return FinishSettingItemSetup(item, key_path, value);
  }

  SettingsItem CreateSettingItem(const std::string& key_path,
                                 PresenceValue value,
                                 const double setting_value) {
    SettingsItem item;
    item.setting_json_value = base::StringPrintf("%f", setting_value);
    return FinishSettingItemSetup(item, key_path, value);
  }

  SettingsItem FinishSettingItemSetup(SettingsItem item,
                                      const std::string& key_path,
                                      PresenceValue value) {
    item.path = test_file_path_.value();
    item.key = key_path;
    item.presence = value;
    return item;
  }

  PlistSettingsClient client_;
  base::FilePath test_file_path_;
  base::test::TaskEnvironment task_environment_;
  base::test::TestFuture<const std::vector<SettingsItem>&> future_;
};

// Tests an empty request to GetSettings. In this case we should have no setting
// items.
TEST_F(PlistSettingsClientTest, GetSettings_EmptyOptions) {
  client_.GetSettings(std::vector<GetSettingsOptions>(), future_.GetCallback());
  EXPECT_EQ(std::vector<SettingsItem>(), future_.Get());
}

// Tests when the request to GetSettings contains one invalid request. In this
// case we should have no setting items.
TEST_F(PlistSettingsClientTest, GetSettings_InvalidOptions) {
  test_file_path_ = test::GetMixArrayDictionaryPlistPath();

  std::vector<GetSettingsOptions> options;
  options.push_back(CreateOption("", false));

  std::vector<SettingsItem> items;
  items.push_back(CreateSettingItem("", PresenceValue::kNotFound, ""));

  client_.GetSettings(options, future_.GetCallback());
  EXPECT_EQ(items, future_.Get());
}

// Tests a request to GetSettings from a plist that does not exist.
TEST_F(PlistSettingsClientTest, GetSettings_PlistDoesNotExist) {
  test_file_path_ =
      test::GetTestDataDir().AppendASCII("plist-doesnt-exist.plist");

  std::vector<GetSettingsOptions> options;
  std::string key_path = "Key1.SubKey1.SubSubKey1[0]";
  options.push_back(CreateOption(key_path, true));

  std::vector<SettingsItem> items;
  items.push_back(CreateSettingItem(key_path, PresenceValue::kNotFound, ""));

  client_.GetSettings(options, future_.GetCallback());
  EXPECT_EQ(items, future_.Get());
}

// Tests a request to GetSettings from an empty plist.
TEST_F(PlistSettingsClientTest, GetSettings_Plist_EmptyPlist) {
  test_file_path_ = test::GetEmptyPlistPath();

  std::vector<GetSettingsOptions> options;
  std::string key_path = "Key1.SubKey1";
  options.push_back(CreateOption(key_path, true));

  std::vector<SettingsItem> items;
  items.push_back(CreateSettingItem(key_path, PresenceValue::kNotFound, ""));

  client_.GetSettings(options, future_.GetCallback());
  EXPECT_EQ(items, future_.Get());
}

// Tests a request to GetSettings from a plist containing a mix of dictionaries
// and arrays with nested key paths that do not exist in the plist.
TEST_F(PlistSettingsClientTest,
       GetSettings_Plist_MixOfArrayDictItems_InvalidKeyPath) {
  test_file_path_ = test::GetMixArrayDictionaryPlistPath();

  std::string key_path1 = "Key1.SubKey1.SubSubKey1[0][20]";
  std::string key_path2 = "Key1.SubKey1.SubSubKey5[1";
  std::string key_path3 = "Key1.Array]";
  std::string key_path4 = "Key1.Array[1000000][";
  std::string key_path5 = "Key1..";
  std::string key_path6 = "Key1.Array[a]";
  std::string key_path7 = "Key1.Array[";
  std::string key_path8 = "Key1.Array[1]]";
  std::string key_path9 = "Key1.Array[]";

  std::vector<GetSettingsOptions> options;
  options.push_back(CreateOption(key_path1, true));
  options.push_back(CreateOption(key_path2, false));
  options.push_back(CreateOption(key_path3, false));
  options.push_back(CreateOption(key_path4, true));
  options.push_back(CreateOption(key_path5, true));
  options.push_back(CreateOption(key_path6, false));
  options.push_back(CreateOption(key_path7, false));
  options.push_back(CreateOption(key_path8, true));
  options.push_back(CreateOption(key_path9, true));

  std::vector<SettingsItem> items;
  items.push_back(CreateSettingItem(key_path1, PresenceValue::kNotFound, ""));
  items.push_back(CreateSettingItem(key_path2, PresenceValue::kNotFound, ""));
  items.push_back(CreateSettingItem(key_path3, PresenceValue::kNotFound, ""));
  items.push_back(CreateSettingItem(key_path4, PresenceValue::kNotFound, ""));
  items.push_back(CreateSettingItem(key_path5, PresenceValue::kNotFound, ""));
  items.push_back(CreateSettingItem(key_path6, PresenceValue::kNotFound, ""));
  items.push_back(CreateSettingItem(key_path7, PresenceValue::kNotFound, ""));
  items.push_back(CreateSettingItem(key_path8, PresenceValue::kNotFound, ""));
  items.push_back(CreateSettingItem(key_path9, PresenceValue::kNotFound, ""));

  client_.GetSettings(options, future_.GetCallback());
  EXPECT_EQ(items, future_.Get());
}

// Tests a request to GetSettings from a plist containing a mix of dictionaries
// and arrays with a nested key path that exists in the plist.
TEST_F(PlistSettingsClientTest,
       GetSettings_Plist_MixOfArrayDictItems_ValidKeyPath) {
  test_file_path_ = test::GetMixArrayDictionaryPlistPath();

  std::string key_path = "Key1.SubKey1.SubSubKey1[0][10]";

  std::vector<GetSettingsOptions> options;
  options.push_back(CreateOption(key_path, true));
  options.push_back(CreateOption(key_path, false));

  std::vector<SettingsItem> items;
  items.push_back(
      CreateSettingItem(key_path, PresenceValue::kFound, "\"string10\""));
  items.push_back(CreateSettingItem(key_path, PresenceValue::kFound, ""));

  client_.GetSettings(options, future_.GetCallback());
  EXPECT_EQ(items, future_.Get());
}

// Tests a request to GetSettings with an invalid setting path from a plist
// containing only dictionary elements.
TEST_F(PlistSettingsClientTest,
       GetSettings_Plist_OnlyDictionaryItems_InvalidKeyPath) {
  test_file_path_ = test::GetOnlyDictionaryPlistPath();

  std::string key_path1 = "Key1.InvalidKeyValue";
  std::string key_path2 = "Key1.InvalidKeyValue.SubKey1";
  std::string key_path3 = "Key1.SubKey1.SubKey2.SubKey3.SubKey4";

  std::vector<GetSettingsOptions> options;
  options.push_back(CreateOption(key_path1, true));
  options.push_back(CreateOption(key_path2, false));
  options.push_back(CreateOption(key_path3, true));

  std::vector<SettingsItem> items;
  items.push_back(CreateSettingItem(key_path1, PresenceValue::kNotFound, ""));
  items.push_back(CreateSettingItem(key_path2, PresenceValue::kNotFound, ""));
  items.push_back(CreateSettingItem(key_path3, PresenceValue::kNotFound, ""));

  client_.GetSettings(options, future_.GetCallback());
  EXPECT_EQ(items, future_.Get());
}

// Tests a request to GetSettings with a path to a data type setting value from
// a plist containing only dictionary elements.
TEST_F(PlistSettingsClientTest,
       GetSettings_Plist_OnlyDictionaryItems_DataType) {
  test_file_path_ = test::GetOnlyDictionaryPlistPath();

  std::string key_path = "DataKeyValue";

  std::vector<GetSettingsOptions> options;
  options.push_back(CreateOption(key_path, true));

  std::vector<SettingsItem> items;
  items.push_back(CreateSettingItem(key_path, PresenceValue::kFound, ""));

  client_.GetSettings(options, future_.GetCallback());
  EXPECT_EQ(items, future_.Get());
}

// Tests a request to GetSettings with a path to a float type setting value from
// a plist containing only dictionary elements.
TEST_F(PlistSettingsClientTest,
       GetSettings_Plist_OnlyDictionaryItems_FloatType) {
  test_file_path_ = test::GetOnlyDictionaryPlistPath();

  std::string key_path = "RealKeyValue";

  std::vector<GetSettingsOptions> options;
  options.push_back(CreateOption(key_path, true));

  std::vector<SettingsItem> items;
  items.push_back(CreateSettingItem(key_path, PresenceValue::kFound, 0.123));

  client_.GetSettings(options, future_.GetCallback());
  EXPECT_EQ(items, future_.Get());
}

// Tests a request to GetSettings with a path to a integer type setting value
// from a plist containing only dictionary elements.
TEST_F(PlistSettingsClientTest,
       GetSettings_Plist_OnlyDictionaryItems_IntegerType) {
  test_file_path_ = test::GetOnlyDictionaryPlistPath();

  std::string key_path = "Key1.SubKey1.IntegerKeyValue";

  std::vector<GetSettingsOptions> options;
  options.push_back(CreateOption(key_path, true));

  std::vector<SettingsItem> items;
  items.push_back(CreateSettingItem(key_path, PresenceValue::kFound, 100));

  client_.GetSettings(options, future_.GetCallback());
  EXPECT_EQ(items, future_.Get());
}

// Tests a request to GetSettings with a path to a string type setting value
// from a plist containing only dictionary elements.
TEST_F(PlistSettingsClientTest,
       GetSettings_Plist_OnlyDictionaryItems_StringType) {
  test_file_path_ = test::GetOnlyDictionaryPlistPath();

  std::string key_path = "Key1.SubKey1.StringKeyValue";

  std::vector<GetSettingsOptions> options;
  options.push_back(CreateOption(key_path, true));

  std::vector<SettingsItem> items;
  items.push_back(
      CreateSettingItem(key_path, PresenceValue::kFound, "\"string\""));

  client_.GetSettings(options, future_.GetCallback());
  EXPECT_EQ(items, future_.Get());
}

// Tests a request to GetSettings with a path to a boolean type setting value
// from a plist containing only dictionary elements.
TEST_F(PlistSettingsClientTest,
       GetSettings_Plist_OnlyDictionaryItems_BooleanType) {
  test_file_path_ = test::GetOnlyDictionaryPlistPath();

  std::string key_path = "Key1.BooleanKeyValue";

  std::vector<GetSettingsOptions> options;
  options.push_back(CreateOption(key_path, true));

  std::vector<SettingsItem> items;
  items.push_back(CreateSettingItem(key_path, PresenceValue::kFound, 1));

  client_.GetSettings(options, future_.GetCallback());
  EXPECT_EQ(items, future_.Get());
}

}  // namespace device_signals
