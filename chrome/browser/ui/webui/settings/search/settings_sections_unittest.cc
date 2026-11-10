// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/settings_sections.h"

#include <memory>
#include <vector>

#include "chrome/browser/ui/webui/settings/search/mojom/setting_id.mojom.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_item.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_registry.h"
#include "chrome/browser/ui/webui/settings/search/settings_section.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings_search {

namespace {

using ::testing::Contains;
using ::testing::Pointee;
using ::testing::Property;

TEST(SettingsSectionsTest, CreateSettingsSectionsRegistersSections) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  SettingsSearchRegistry registry;
  std::vector<std::unique_ptr<SettingsSection>> sections =
      CreateSettingsSections(&registry, &profile);

  EXPECT_THAT(registry.GetItems(),
              Contains(Pointee(Property(&SettingsSearchItem::id,
                                        mojom::SettingId::kAppearance))));
}

}  // namespace

}  // namespace settings_search
