// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/password_manager_handler.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"

namespace {

class PasswordManagerHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    InitializeActionIdStringMapping();
  }

  void TearDown() override {
    actions::ActionIdMap::ResetMapsForTesting();
    testing::Test::TearDown();
  }

 protected:
  TestingProfile& profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(PasswordManagerHandlerTest, PinPasswordManager) {
  mojo::PendingReceiver<feature_showcase::mojom::PasswordManagerPageHandler>
      receiver;
  PasswordManagerHandler handler(std::move(receiver), &profile());

  PinnedToolbarActionsModel* model = PinnedToolbarActionsModel::Get(&profile());
  ASSERT_TRUE(model);

  ASSERT_FALSE(model->Contains(kActionShowPasswordsBubbleOrPage));
  handler.PinPasswordManager();
  EXPECT_TRUE(model->Contains(kActionShowPasswordsBubbleOrPage));
}

}  // namespace
