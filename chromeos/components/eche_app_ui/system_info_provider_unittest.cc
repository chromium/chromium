// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/eche_app_ui/system_info_provider.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chromeos/components/eche_app_ui/system_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace eche_app {

const char kFakeDeviceName[] = "Guanru's Chromebook";
const char kFakeBoardName[] = "atlas";

void ParseJson(const std::string& json,
               std::string& device_name,
               std::string& board_name) {
  std::unique_ptr<base::Value> message_value =
      base::JSONReader::ReadDeprecated(json);
  base::DictionaryValue* message_dictionary;
  message_value->GetAsDictionary(&message_dictionary);
  message_dictionary->GetString(kJsonDeviceNameKey, &device_name);
  message_dictionary->GetString(kJsonBoardNameKey, &board_name);
}

class Callback {
 public:
  static void GetSystemInfoCallback(const std::string& system_info) {
    system_info_ = system_info;
  }
  static std::string GetSystemInfo() { return system_info_; }
  static void resetSystemInfo() { system_info_ = ""; }

 private:
  static std::string system_info_;
};

std::string chromeos::eche_app::Callback::system_info_ = "";

class SystemInfoProviderTest : public testing::Test {
 protected:
  SystemInfoProviderTest() = default;
  SystemInfoProviderTest(const SystemInfoProviderTest&) = delete;
  SystemInfoProviderTest& operator=(const SystemInfoProviderTest&) = delete;
  ~SystemInfoProviderTest() override = default;

  // testing::Test:
  void SetUp() override {
    system_info_provider_ =
        std::make_unique<SystemInfoProvider>(SystemInfo::Builder()
                                                 .SetDeviceName(kFakeDeviceName)
                                                 .SetBoardName(kFakeBoardName)
                                                 .Build());
  }
  void TearDown() override {
    system_info_provider_.reset();
    Callback::resetSystemInfo();
  }
  void GetSystemInfo() {
    system_info_provider_->GetSystemInfo(
        base::BindOnce(&Callback::GetSystemInfoCallback));
  }

 private:
  std::unique_ptr<SystemInfoProvider> system_info_provider_;
};

TEST_F(SystemInfoProviderTest, GetSystemInfoHasCorrectJson) {
  std::string device_name = "";
  std::string board_name = "";

  GetSystemInfo();
  std::string json = Callback::GetSystemInfo();
  ParseJson(json, device_name, board_name);

  EXPECT_EQ(device_name, kFakeDeviceName);
  EXPECT_EQ(board_name, kFakeBoardName);
}

}  // namespace eche_app
}  // namespace chromeos
