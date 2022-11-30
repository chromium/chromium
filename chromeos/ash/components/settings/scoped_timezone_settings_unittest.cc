// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/settings/scoped_timezone_settings.h"

#include <memory>
#include <string>

#include "chromeos/ash/components/settings/timezone_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::system {
namespace {

std::u16string GetCurrentTimezoneIDFromTimezoneSettings() {
  return TimezoneSettings::GetInstance()->GetCurrentTimezoneID();
}

TEST(ScopedTimezoneSettingsTest, CreatesInstanceViaParameterlessConstructor) {
  std::u16string original_timezone = GetCurrentTimezoneIDFromTimezoneSettings();

  // Parameterless constructor, timezone stays the same.
  auto scoped_timezone_settings = std::make_unique<ScopedTimezoneSettings>();
  EXPECT_EQ(GetCurrentTimezoneIDFromTimezoneSettings(), original_timezone);
  EXPECT_EQ(GetCurrentTimezoneIDFromTimezoneSettings(),
            scoped_timezone_settings->GetCurrentTimezoneID());

  // Updates via `SetTimezoneFromID`.
  scoped_timezone_settings->SetTimezoneFromID(u"GMT+01:00");
  EXPECT_EQ(GetCurrentTimezoneIDFromTimezoneSettings(), u"GMT+01:00");
  EXPECT_EQ(GetCurrentTimezoneIDFromTimezoneSettings(),
            scoped_timezone_settings->GetCurrentTimezoneID());

  // Resets the timezone back in destructor.
  scoped_timezone_settings.reset();
  EXPECT_EQ(GetCurrentTimezoneIDFromTimezoneSettings(), original_timezone);
}

TEST(ScopedTimezoneSettingsTest, CreatesInstanceViaParameterizedConstructor) {
  std::u16string original_timezone = GetCurrentTimezoneIDFromTimezoneSettings();

  // Sets a new timezone in constructor.
  auto scoped_timezone_settings =
      std::make_unique<ScopedTimezoneSettings>(u"Atlantic/Azores");
  EXPECT_EQ(GetCurrentTimezoneIDFromTimezoneSettings(), u"Atlantic/Azores");
  EXPECT_EQ(GetCurrentTimezoneIDFromTimezoneSettings(),
            scoped_timezone_settings->GetCurrentTimezoneID());

  // Also updates via `SetTimezoneFromID`.
  scoped_timezone_settings->SetTimezoneFromID(u"GMT+01:00");
  EXPECT_EQ(GetCurrentTimezoneIDFromTimezoneSettings(), u"GMT+01:00");
  EXPECT_EQ(GetCurrentTimezoneIDFromTimezoneSettings(),
            scoped_timezone_settings->GetCurrentTimezoneID());

  // Resets the timezone back in destructor.
  scoped_timezone_settings.reset();
  EXPECT_EQ(GetCurrentTimezoneIDFromTimezoneSettings(), original_timezone);
}

}  // namespace
}  // namespace ash::system
