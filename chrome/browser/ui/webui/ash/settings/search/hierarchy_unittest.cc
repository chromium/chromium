// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/search/hierarchy.h"

#include <sstream>

#include "ash/webui/settings/public/constants/routes.mojom-shared.h"
#include "chrome/browser/ui/webui/ash/settings/test_support/fake_os_settings_sections.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

namespace {

class HierarchyTest : public testing::Test {
 protected:
  HierarchyTest() { fake_sections_.FillWithFakeSettings(); }

  ~HierarchyTest() override = default;

  FakeOsSettingsSections fake_sections_;
};

TEST_F(HierarchyTest, Init) {
  // Should successfully initialize with fake data.
  Hierarchy h(&fake_sections_);

#ifdef DCHECK
  // Should print out some text with debug output.
  std::stringstream os;
  os << h;
  ASSERT_GT(os.str().size(), 0u);
#endif

  // Should contain "About" section.
  h.GetSectionMetadata(chromeos::settings::mojom::Section::kAboutChromeOs);
}

}  // namespace

}  // namespace ash::settings
