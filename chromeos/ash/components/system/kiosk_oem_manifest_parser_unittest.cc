// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/kiosk_oem_manifest_parser.h"

#include "chromeos/test/chromeos_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

typedef testing::Test KioskOemManifestParserTest;

TEST_F(KioskOemManifestParserTest, LoadTest) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(chromeos::test_utils::GetTestDataPath(
      "app_mode", "kiosk_manifest", &test_data_dir));
  base::FilePath kiosk_oem_file =
      test_data_dir.AppendASCII("kiosk_manifest.json");
  KioskOemManifestParser::Manifest manifest;
  EXPECT_TRUE(KioskOemManifestParser::Load(kiosk_oem_file, &manifest));
  EXPECT_TRUE(manifest.enterprise_managed);
  EXPECT_FALSE(manifest.can_exit_enrollment);
  EXPECT_TRUE(manifest.keyboard_driven_oobe);
  EXPECT_EQ(manifest.device_requisition, std::string("test"));
}

}  // namespace ash
