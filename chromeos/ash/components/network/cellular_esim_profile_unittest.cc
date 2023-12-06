// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_esim_profile.h"

#include <optional>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(CellularESimProfileTest, ConvertToAndFromDictionary) {
  CellularESimProfile profile(CellularESimProfile::State::kPending,
                              dbus::ObjectPath("/test/path/123"), "eid",
                              "iccid", u"name", u"nickname", u"serviceProvider",
                              "activationCode");

  base::Value::Dict dictionary = profile.ToDictionaryValue();
  std::optional<CellularESimProfile> from_dictionary =
      CellularESimProfile::FromDictionaryValue(dictionary);
  EXPECT_TRUE(from_dictionary);

  EXPECT_EQ(CellularESimProfile::State::kPending, from_dictionary->state());
  EXPECT_EQ(dbus::ObjectPath("/test/path/123"), from_dictionary->path());
  EXPECT_EQ("eid", from_dictionary->eid());
  EXPECT_EQ("iccid", from_dictionary->iccid());
  EXPECT_EQ(u"name", from_dictionary->name());
  EXPECT_EQ(u"nickname", from_dictionary->nickname());
  EXPECT_EQ(u"serviceProvider", from_dictionary->service_provider());
  EXPECT_EQ("activationCode", from_dictionary->activation_code());
}

TEST(CellularESimProfileTest, InvalidDictionary) {
  // Try to convert a dictionary without the required keys.
  auto dictionary = base::Value::Dict().Set("sampleKey", "sampleValue");
  std::optional<CellularESimProfile> from_dictionary =
      CellularESimProfile::FromDictionaryValue(dictionary);
  EXPECT_FALSE(from_dictionary);
}

}  // namespace ash
