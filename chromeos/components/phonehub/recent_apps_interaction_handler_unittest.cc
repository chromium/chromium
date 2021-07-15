// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/recent_apps_interaction_handler.h"

#include <memory>

#include "chromeos/components/phonehub/notification.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {

class FakeClickHandler : public RecentAppClickObserver {
 public:
  FakeClickHandler() = default;
  ~FakeClickHandler() override = default;

  std::u16string get_visible_app_name() { return visible_app_name; }

  std::string get_package_name() { return package_name; }

  void OnRecentAppClicked(
      const Notification::AppMetadata& app_metadata) override {
    visible_app_name = app_metadata.visible_app_name;
    package_name = app_metadata.package_name;
  }

 private:
  std::u16string visible_app_name;
  std::string package_name;
};

}  // namespace

class RecentAppsInteractionHandlerTest : public testing::Test {
 protected:
  RecentAppsInteractionHandlerTest() = default;
  RecentAppsInteractionHandlerTest(const RecentAppsInteractionHandlerTest&) =
      delete;
  RecentAppsInteractionHandlerTest& operator=(
      const RecentAppsInteractionHandlerTest&) = delete;
  ~RecentAppsInteractionHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    interaction_handler_ = std::make_unique<RecentAppsInteractionHandler>();
    interaction_handler_->AddRecentAppClickObserver(&fake_click_handler_);
  }

  void TearDown() override {
    interaction_handler_->RemoveRecentAppClickObserver(&fake_click_handler_);
  }

  std::u16string GetVisibleAppName() {
    return fake_click_handler_.get_visible_app_name();
  }

  std::string GetPackageName() {
    return fake_click_handler_.get_package_name();
  }

  RecentAppsInteractionHandler& handler() { return *interaction_handler_; }

 private:
  FakeClickHandler fake_click_handler_;
  std::unique_ptr<RecentAppsInteractionHandler> interaction_handler_;
};

TEST_F(RecentAppsInteractionHandlerTest, NotifyRecentAppsClickHandler) {
  const char16_t expected_app_visible_name[] = u"Fake App";
  const char expected_package_name[] = "com.fakeapp";
  auto expected_app_metadata = Notification::AppMetadata(
      expected_app_visible_name, expected_package_name, gfx::Image());

  handler().NotifyRecentAppClicked(expected_app_metadata);

  EXPECT_EQ(expected_app_visible_name, GetVisibleAppName());
  EXPECT_EQ(expected_package_name, GetPackageName());
}

}  // namespace phonehub
}  // namespace chromeos
