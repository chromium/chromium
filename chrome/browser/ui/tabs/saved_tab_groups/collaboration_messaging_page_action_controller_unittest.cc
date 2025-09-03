// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_page_action_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_tab_data.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "memory"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "url/gurl.h"

using collaboration::messaging::CollaborationEvent;
using collaboration::messaging::MessageAttribution;
using collaboration::messaging::PersistentMessage;
using collaboration::messaging::PersistentNotificationType;
using data_sharing::GroupMember;
using ::testing::_;

namespace {

constexpr char kGivenName[] = "User";
constexpr char kAvatarUrl[] = "https://google.com/chrome/1";

PersistentMessage CreateMessage(const std::string& given_name,
                                const std::string& avatar_url,
                                CollaborationEvent event) {
  GroupMember member;
  member.given_name = given_name;
  member.avatar_url = GURL(avatar_url);

  MessageAttribution attribution;
  attribution.triggering_user = member;

  PersistentMessage message;
  message.type = PersistentNotificationType::CHIP;
  message.attribution = attribution;
  message.collaboration_event = event;

  return message;
}

const std::u16string TabAddedLabel() {
  return l10n_util::GetStringUTF16(IDS_DATA_SHARING_PAGE_ACTION_ADDED_NEW_TAB);
}

const std::u16string TabUpdatedLabel() {
  return l10n_util::GetStringUTF16(IDS_DATA_SHARING_PAGE_ACTION_CHANGED_TAB);
}

class FakeTabInterface : public tabs::MockTabInterface {
 public:
  ~FakeTabInterface() override = default;
  explicit FakeTabInterface(std::unique_ptr<content::WebContents> contents)
      : contents_(std::move(contents)) {}
  content::WebContents* GetContents() const override { return contents_.get(); }

  bool IsActivated() const override { return activated_; }

  void SetTabActivation(bool activated) { activated_ = activated; }

 private:
  std::unique_ptr<content::WebContents> contents_;
  bool activated_ = true;
};

class FakePageActionController : public page_actions::MockPageActionController {
 public:
  FakePageActionController() = default;
  ~FakePageActionController() override = default;

  void OverrideImage(actions::ActionId action_id,
                     const ui::ImageModel& image) override {
    page_actions::MockPageActionController::OverrideImage(action_id, image);
    image_set_ = true;
  }

  void ClearOverrideImage(actions::ActionId action_id) override {
    page_actions::MockPageActionController::ClearOverrideImage(action_id);
    image_set_ = false;
  }

  void OverrideText(actions::ActionId action_id,
                    const std::u16string& text) override {
    page_actions::MockPageActionController::OverrideText(action_id, text);
    last_text_ = text;
  }

  void ClearOverrideText(actions::ActionId action_id) override {
    page_actions::MockPageActionController::ClearOverrideText(action_id);
    last_text_ = u"";
  }

  const std::u16string& last_text() const { return last_text_; }
  bool is_image_set() const { return image_set_; }

 private:
  std::u16string last_text_;
  bool image_set_ = false;
};
}  // namespace

class CollaborationMessagingPageActionControllerTest : public testing::Test {
 public:
  CollaborationMessagingPageActionControllerTest() = default;
  ~CollaborationMessagingPageActionControllerTest() override = default;

 protected:
  void SetUp() override {
    Test::SetUp();

    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {data_sharing::features::kDataSharingFeature, {}},
        {
            features::kPageActionsMigration,
            {
                {
                    features::kPageActionsMigrationCollaborationMessaging.name,
                    "true",
                },
            },
        }};

    features_.InitWithFeaturesAndParameters(enabled_features, {});

    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

    tab_interface_ =
        std::make_unique<FakeTabInterface>(std::move(web_contents));

    EXPECT_CALL(*tab_interface_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(data_host_));

    EXPECT_CALL(*tab_interface_, GetBrowserWindowInterface())
        .Times(1)
        .WillRepeatedly(testing::Return(&mock_browser_window_interface_));

    EXPECT_CALL(mock_browser_window_interface_, GetProfile())
        .Times(1)
        .WillRepeatedly(testing::Return(profile()));

    tab_data_ = std::make_unique<tab_groups::CollaborationMessagingTabData>(
        tab_interface());

    controller_ = std::make_unique<CollaborationMessagingPageActionController>(
        *tab_interface_, page_action_controller(), *tab_data_);
  }

  TestingProfile* profile() { return &profile_; }

  CollaborationMessagingPageActionController* controller() {
    return controller_.get();
  }

  tab_groups::CollaborationMessagingTabData* tab_data() {
    return tab_data_.get();
  }

  FakePageActionController& page_action_controller() {
    return page_action_controller_;
  }

  FakeTabInterface* tab_interface() { return tab_interface_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_enabler_;
  base::test::ScopedFeatureList features_;
  TestingProfile profile_;
  ui::UnownedUserDataHost data_host_;
  std::unique_ptr<FakeTabInterface> tab_interface_;
  std::unique_ptr<tab_groups::CollaborationMessagingTabData> tab_data_;
  std::unique_ptr<CollaborationMessagingPageActionController> controller_;
  FakePageActionController page_action_controller_;
  MockBrowserWindowInterface mock_browser_window_interface_;
};

TEST_F(CollaborationMessagingPageActionControllerTest,
       TabAddedLabelShouldMatch) {
  EXPECT_CALL(page_action_controller(),
              Show(kActionShowCollaborationRecentActivity))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCollaborationRecentActivity, _))
      .Times(1);
  EXPECT_CALL(
      page_action_controller(),
      OverrideTooltip(kActionShowCollaborationRecentActivity, TabAddedLabel()))
      .Times(1);
  EXPECT_CALL(
      page_action_controller(),
      OverrideText(kActionShowCollaborationRecentActivity, TabAddedLabel()))
      .Times(1);

  auto message =
      CreateMessage(kGivenName, kAvatarUrl, CollaborationEvent::TAB_ADDED);

  tab_data()->set_mocked_avatar_for_testing(favicon::GetDefaultFavicon());
  tab_data()->SetMessage(message);

  EXPECT_EQ(page_action_controller().last_text(), TabAddedLabel());
}

TEST_F(CollaborationMessagingPageActionControllerTest,
       TabUpdatedLabelShouldMatch) {
  EXPECT_CALL(page_action_controller(),
              Show(kActionShowCollaborationRecentActivity))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCollaborationRecentActivity, _))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              OverrideTooltip(kActionShowCollaborationRecentActivity, _))
      .Times(1);
  EXPECT_CALL(
      page_action_controller(),
      OverrideText(kActionShowCollaborationRecentActivity, TabUpdatedLabel()))
      .Times(1);

  auto message =
      CreateMessage(kGivenName, kAvatarUrl, CollaborationEvent::TAB_UPDATED);

  tab_data()->set_mocked_avatar_for_testing(favicon::GetDefaultFavicon());
  tab_data()->SetMessage(message);

  EXPECT_EQ(page_action_controller().last_text(), TabUpdatedLabel());
}

TEST_F(CollaborationMessagingPageActionControllerTest, AvatarShouldDraw) {
  EXPECT_CALL(page_action_controller(),
              Show(kActionShowCollaborationRecentActivity))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCollaborationRecentActivity, _))
      .Times(1);

  auto message =
      CreateMessage(kGivenName, kAvatarUrl, CollaborationEvent::TAB_ADDED);

  EXPECT_FALSE(page_action_controller().is_image_set());

  tab_data()->set_mocked_avatar_for_testing(favicon::GetDefaultFavicon());
  tab_data()->SetMessage(message);

  EXPECT_TRUE(page_action_controller().is_image_set());
}

TEST_F(CollaborationMessagingPageActionControllerTest, IconShouldHide) {
  auto message =
      CreateMessage(kGivenName, kAvatarUrl, CollaborationEvent::TAB_ADDED);

  EXPECT_FALSE(page_action_controller().is_image_set());

  tab_data()->set_mocked_avatar_for_testing(favicon::GetDefaultFavicon());
  tab_data()->SetMessage(message);

  EXPECT_TRUE(page_action_controller().is_image_set());

  EXPECT_CALL(page_action_controller(),
              HideSuggestionChip(kActionShowCollaborationRecentActivity))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              Hide(kActionShowCollaborationRecentActivity))
      .Times(1);

  tab_data()->set_mocked_avatar_for_testing(gfx::Image());
  tab_data()->ClearMessage(message);

  EXPECT_FALSE(page_action_controller().is_image_set());
}

TEST_F(CollaborationMessagingPageActionControllerTest,
       PageActionDoesNotShowOnInactiveTab) {
  EXPECT_CALL(page_action_controller(),
              Show(kActionShowCollaborationRecentActivity))
      .Times(0);
  EXPECT_CALL(page_action_controller(),
              ShowSuggestionChip(kActionShowCollaborationRecentActivity, _))
      .Times(0);

  auto message =
      CreateMessage(kGivenName, kAvatarUrl, CollaborationEvent::TAB_ADDED);

  tab_interface()->SetTabActivation(false);
  tab_data()->set_mocked_avatar_for_testing(favicon::GetDefaultFavicon());
  tab_data()->SetMessage(message);
}
