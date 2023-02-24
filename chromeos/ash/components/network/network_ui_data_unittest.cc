// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_ui_data.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(NetworkUIDataTest, ONCSource) {
  base::Value::Dict ui_data_dict;

  ui_data_dict.Set("onc_source", "user_import");
  {
    NetworkUIData ui_data(ui_data_dict);
    EXPECT_EQ(::onc::ONC_SOURCE_USER_IMPORT, ui_data.onc_source());
  }

  ui_data_dict.Set("onc_source", "device_policy");
  {
    NetworkUIData ui_data(ui_data_dict);
    EXPECT_EQ(::onc::ONC_SOURCE_DEVICE_POLICY, ui_data.onc_source());
  }
  ui_data_dict.Set("onc_source", "user_policy");
  {
    NetworkUIData ui_data(ui_data_dict);
    EXPECT_EQ(::onc::ONC_SOURCE_USER_POLICY, ui_data.onc_source());
  }
}

}  // namespace ash
