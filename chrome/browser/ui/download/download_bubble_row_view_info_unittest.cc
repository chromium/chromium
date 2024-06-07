// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_row_view_info.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/vector_icons.h"

namespace {

void SetOnCall(bool* to_set) {
  *to_set = true;
}

using offline_items_collection::ContentId;
using DownloadState = download::DownloadItem::DownloadState;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;
using ::testing::UnorderedElementsAre;

#if BUILDFLAG(FULL_SAFE_BROWSING)
using TailoredVerdict = safe_browsing::ClientDownloadResponse::TailoredVerdict;
#endif

class DownloadBubbleRowViewInfoTest : public testing::Test,
                                      public DownloadBubbleRowViewInfoObserver {
 public:
  DownloadBubbleRowViewInfoTest() = default;

  void SetUp() override {
    if (!download::IsDownloadBubbleEnabled()) {
      GTEST_SKIP();
    }
    item_ = std::make_unique<NiceMock<download::MockDownloadItem>>();
    ON_CALL(*item_, GetGuid())
        .WillByDefault(ReturnRefOfCopy(std::string("id")));
    ON_CALL(*item_, GetURL())
        .WillByDefault(ReturnRefOfCopy(GURL("http://example.com/foo.bar")));
    content::DownloadItemUtils::AttachInfoForTesting(item_.get(), &profile_,
                                                     nullptr);
    info_ = std::make_unique<DownloadBubbleRowViewInfo>(
        DownloadItemModel::Wrap(item_.get()));
  }

  void DestroyItem() { item_.reset(); }

  NiceMock<download::MockDownloadItem>& item() { return *item_; }

  Profile* profile() { return &profile_; }

  ContentId content_id() {
    return OfflineItemUtils::GetContentIdForDownload(item_.get());
  }

  DownloadBubbleRowViewInfo& info() { return *info_; }

  void SetInfoChangedCallback(base::OnceClosure callback) {
    on_info_changed_ = std::move(callback);
  }

  void SetDownloadStateChangedCallback(
      base::OnceCallback<void(DownloadState, DownloadState)> callback) {
    on_download_state_changed_ = std::move(callback);
  }

  void SetDownloadDestroyedCallback(
      base::OnceCallback<void(const ContentId&)> callback) {
    on_download_destroyed_ = std::move(callback);
  }

  void SetupTailoredWarningForItem(
      download::DownloadDangerType danger_type,
      TailoredVerdict::TailoredVerdictType tailored_verdict_type,
      std::vector<TailoredVerdict::ExperimentalWarningAdjustment> adjustments) {
    ON_CALL(item(), GetDangerType()).WillByDefault(Return(danger_type));
    TailoredVerdict tailored_verdict;
    tailored_verdict.set_tailored_verdict_type(tailored_verdict_type);
    for (const auto& adjustment : adjustments) {
      tailored_verdict.add_adjustments(adjustment);
    }
    safe_browsing::DownloadProtectionService::SetDownloadProtectionData(
        &item(), "token",
        safe_browsing::ClientDownloadResponse::SAFE,  // placeholder
        tailored_verdict);
  }

 private:
  // DownloadBubbleRowViewInfoObserver implementation:
  void OnInfoChanged() override {
    if (on_info_changed_) {
      std::move(on_info_changed_).Run();
    }
  }
  void OnDownloadDestroyed(
      const offline_items_collection::ContentId& id) override {
    if (on_download_destroyed_) {
      std::move(on_download_destroyed_).Run(id);
    }
  }
  void OnDownloadStateChanged(DownloadState old_state,
                              DownloadState new_state) override {
    if (on_download_state_changed_) {
      std::move(on_download_state_changed_).Run(old_state, new_state);
    }
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<NiceMock<download::MockDownloadItem>> item_;
  std::unique_ptr<DownloadBubbleRowViewInfo> info_;
  base::OnceClosure on_info_changed_;
  base::OnceCallback<void(const ContentId&)> on_download_destroyed_;
  base::OnceCallback<void(DownloadState, DownloadState)>
      on_download_state_changed_;
};

TEST_F(DownloadBubbleRowViewInfoTest, NotifyObserverOnUpdate) {
  info().AddObserver(this);
  bool notified = false;
  SetInfoChangedCallback(base::BindOnce(&SetOnCall, &notified));

  item().NotifyObserversDownloadUpdated();

  EXPECT_TRUE(notified);
}

TEST_F(DownloadBubbleRowViewInfoTest, NotifyObserverOnStateUpdate) {
  EXPECT_CALL(item(), GetState())
      .WillRepeatedly(Return(download::DownloadItem::IN_PROGRESS));
  info().AddObserver(this);
  bool notified = false;
  SetDownloadStateChangedCallback(
      base::IgnoreArgs<DownloadState, DownloadState>(
          base::BindOnce(&SetOnCall, &notified)));

  EXPECT_CALL(item(), GetState())
      .WillRepeatedly(Return(download::DownloadItem::COMPLETE));
  item().NotifyObserversDownloadUpdated();

  EXPECT_TRUE(notified);
}

TEST_F(DownloadBubbleRowViewInfoTest, NotifyObserverOnDestroyed) {
  info().AddObserver(this);
  bool notified = false;
  ContentId expected_id = content_id();
  SetDownloadDestroyedCallback(
      base::BindLambdaForTesting([&](const ContentId& id) {
        EXPECT_EQ(id, expected_id);
        notified = true;
      }));

  DestroyItem();

  EXPECT_TRUE(notified);
}

TEST_F(DownloadBubbleRowViewInfoTest, InsecureDownloadPrimaryCommand) {
  ON_CALL(item(), GetInsecureDownloadStatus())
      .WillByDefault(
          Return(download::DownloadItem::InsecureDownloadStatus::BLOCK));
  item().NotifyObserversDownloadUpdated();
  EXPECT_EQ(info().primary_button_command().value(),
            DownloadCommands::Command::KEEP);

  ON_CALL(item(), GetInsecureDownloadStatus())
      .WillByDefault(
          Return(download::DownloadItem::InsecureDownloadStatus::WARN));
  item().NotifyObserversDownloadUpdated();
  EXPECT_EQ(info().primary_button_command().value(),
            DownloadCommands::Command::KEEP);
}

TEST_F(DownloadBubbleRowViewInfoTest, InProgressOrCompletedInfo) {
  ON_CALL(item(), GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  item().NotifyObserversDownloadUpdated();
  std::vector<DownloadCommands::Command> quick_action_commands;
  for (const auto& quick_action : info().quick_actions()) {
    quick_action_commands.push_back(quick_action.command);
  }
  EXPECT_THAT(
      quick_action_commands,
      UnorderedElementsAre(DownloadCommands::Command::SHOW_IN_FOLDER,
                           DownloadCommands::Command::OPEN_WHEN_COMPLETE));
  EXPECT_FALSE(info().primary_button_command().has_value());

  ON_CALL(item(), GetState())
      .WillByDefault(Return(download::DownloadItem::IN_PROGRESS));
  EXPECT_CALL(item(), IsPaused()).WillRepeatedly(Return(true));
  item().NotifyObserversDownloadUpdated();
  quick_action_commands.clear();
  for (const auto& quick_action : info().quick_actions()) {
    quick_action_commands.push_back(quick_action.command);
  }
  EXPECT_THAT(quick_action_commands,
              UnorderedElementsAre(DownloadCommands::Command::RESUME,
                                   DownloadCommands::Command::CANCEL));
  EXPECT_FALSE(info().primary_button_command().has_value());

  EXPECT_CALL(item(), IsPaused()).WillRepeatedly(Return(false));
  item().NotifyObserversDownloadUpdated();
  quick_action_commands.clear();
  for (const auto& quick_action : info().quick_actions()) {
    quick_action_commands.push_back(quick_action.command);
  }
  EXPECT_THAT(quick_action_commands,
              UnorderedElementsAre(DownloadCommands::Command::PAUSE,
                                   DownloadCommands::Command::CANCEL));
  EXPECT_FALSE(info().primary_button_command().has_value());
}

TEST_F(DownloadBubbleRowViewInfoTest, DangerousWarningInfo) {
  ON_CALL(item(), GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  const struct DangerTypeTestCase {
    download::DownloadDangerType danger_type;
    std::optional<DownloadCommands::Command> primary_button_command;
  } kDangerTypeTestCases[] = {
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE, std::nullopt},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT, std::nullopt},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST, std::nullopt},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE,
       std::nullopt},
      {download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED, std::nullopt},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL, std::nullopt},
      {download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING,
       DownloadCommands::Command::DISCARD},
      {download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING, std::nullopt},
      {download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING, std::nullopt},
  };
  for (const auto& test_case : kDangerTypeTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Failed for danger type "
                 << download::GetDownloadDangerTypeString(test_case.danger_type)
                 << std::endl);
    ON_CALL(item(), GetDangerType())
        .WillByDefault(Return(test_case.danger_type));
    item().NotifyObserversDownloadUpdated();
    EXPECT_EQ(info().primary_button_command(),
              test_case.primary_button_command);
    EXPECT_TRUE(info().has_subpage());
  }
}

TEST_F(DownloadBubbleRowViewInfoTest, InterruptedInfo) {
  std::vector<download::DownloadInterruptReason> no_retry_interrupt_reasons = {
      download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT};
  std::vector<download::DownloadInterruptReason> retry_interrupt_reasons = {
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED,
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT,
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED,
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR,
      download::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN,
      download::DOWNLOAD_INTERRUPT_REASON_CRASH,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_CROSS_ORIGIN_REDIRECT,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT};

  const struct TestCase {
    // Inputs to the test
    std::vector<download::DownloadInterruptReason> interrupt_reasons;
    bool can_resume;
    // Test expectations
    raw_ptr<const gfx::VectorIcon> expected_icon_model_override;
    std::optional<DownloadCommands::Command> expected_primary_button_command;
  } kTestCases[] = {
      {{download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED},
       false,
       &views::kInfoChromeRefreshIcon,
       std::optional<DownloadCommands::Command>()},
      {{download::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG},
       false,
       &vector_icons::kFileDownloadOffChromeRefreshIcon,
       std::optional<DownloadCommands::Command>()},
      {{download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE},
       false,
       &vector_icons::kFileDownloadOffChromeRefreshIcon,
       std::optional<DownloadCommands::Command>()},
      {{download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED},
       false,
       &vector_icons::kFileDownloadOffChromeRefreshIcon,
       std::optional<DownloadCommands::Command>()},
      {no_retry_interrupt_reasons, false,
       &vector_icons::kFileDownloadOffChromeRefreshIcon,
       std::optional<DownloadCommands::Command>()},
      {retry_interrupt_reasons, false,
       &vector_icons::kFileDownloadOffChromeRefreshIcon,
       DownloadCommands::Command::RETRY},
      {retry_interrupt_reasons, true,
       &vector_icons::kFileDownloadOffChromeRefreshIcon,
       DownloadCommands::Command::RESUME},
  };

  for (const auto& test_case : kTestCases) {
    for (const auto& interrupt_reason : test_case.interrupt_reasons) {
      SCOPED_TRACE(testing::Message()
                   << "Failed for interrupt reason "
                   << static_cast<int>(interrupt_reason) << std::endl);

      EXPECT_CALL(item(), GetLastReason())
          .WillRepeatedly(Return(interrupt_reason));
      EXPECT_CALL(item(), GetState())
          .WillRepeatedly(Return(download::DownloadItem::INTERRUPTED));
      EXPECT_CALL(item(), CanResume())
          .WillRepeatedly(Return(test_case.can_resume));
      item().NotifyObserversDownloadUpdated();

      EXPECT_EQ(test_case.expected_icon_model_override, info().icon_override());
      EXPECT_EQ(test_case.expected_primary_button_command,
                info().primary_button_command());
      EXPECT_EQ(kColorDownloadItemIconDangerous, info().secondary_color());
    }
  }
}

TEST_F(DownloadBubbleRowViewInfoTest, GetInfoForTailoredWarning_CookieTheft) {
  SetupTailoredWarningForItem(
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE,
      TailoredVerdict::COOKIE_THEFT, /*adjustments=*/{});
  item().NotifyObserversDownloadUpdated();

  // No primary button on download row view. Button only appears on subpage.
  EXPECT_FALSE(info().primary_button_command().has_value());
  EXPECT_TRUE(info().has_subpage());
}

TEST_F(DownloadBubbleRowViewInfoTest,
       GetInfoForTailoredWarning_SuspiciousArchive) {
  SetupTailoredWarningForItem(download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT,
                              TailoredVerdict::SUSPICIOUS_ARCHIVE,
                              /*adjustments=*/{});
  item().NotifyObserversDownloadUpdated();

  // No primary button on download row view. Button only appears on subpage.
  EXPECT_FALSE(info().primary_button_command().has_value());
  EXPECT_TRUE(info().has_subpage());
}

TEST_F(DownloadBubbleRowViewInfoTest,
       GetInfoForTailoredWarning_AccountInfoStringWithAccount) {
  SetupTailoredWarningForItem(
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE,
      TailoredVerdict::COOKIE_THEFT, {TailoredVerdict::ACCOUNT_INFO_STRING});
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  signin::SetPrimaryAccount(identity_manager, "test@example.com",
                            signin::ConsentLevel::kSignin);
  item().NotifyObserversDownloadUpdated();

  // No primary button on download row view. Button only appears on subpage.
  EXPECT_FALSE(info().primary_button_command().has_value());
  EXPECT_TRUE(info().has_subpage());
}

TEST_F(DownloadBubbleRowViewInfoTest,
       GetInfoForTailoredWarning_AccountInfoStringWithoutAccount) {
  SetupTailoredWarningForItem(
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE,
      TailoredVerdict::COOKIE_THEFT, {TailoredVerdict::ACCOUNT_INFO_STRING});
  item().NotifyObserversDownloadUpdated();

  // No primary button on download row view. Button only appears on subpage.
  EXPECT_FALSE(info().primary_button_command().has_value());
  EXPECT_TRUE(info().has_subpage());
}

TEST_F(DownloadBubbleRowViewInfoTest, InsecurePrimaryButtonCommand) {
  for (const auto& insecure_download_status :
       {download::DownloadItem::InsecureDownloadStatus::BLOCK,
        download::DownloadItem::InsecureDownloadStatus::WARN}) {
    ON_CALL(item(), GetInsecureDownloadStatus())
        .WillByDefault(Return(insecure_download_status));
    item().NotifyObserversDownloadUpdated();
    EXPECT_EQ(info().primary_button_command(), DownloadCommands::Command::KEEP);
  }
}

TEST_F(DownloadBubbleRowViewInfoTest,
       ShouldShowNoticeForEnhancedProtectionScan) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(safe_browsing::kDeepScanningPromptRemoval);
  EXPECT_CALL(item(), GetDangerType())
      .WillRepeatedly(
          Return(download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING));
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  EXPECT_TRUE(info().ShouldShowDeepScanNotice());
}

TEST_F(DownloadBubbleRowViewInfoTest,
       ShouldNotShowNoticeForAdvancedProtectionScan) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(safe_browsing::kDeepScanningPromptRemoval);
  EXPECT_CALL(item(), GetDangerType())
      .WillRepeatedly(
          Return(download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING));
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  EXPECT_FALSE(info().ShouldShowDeepScanNotice());
}

TEST_F(DownloadBubbleRowViewInfoTest, ShouldNotShowNoticeWithoutFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(safe_browsing::kDeepScanningPromptRemoval);
  EXPECT_CALL(item(), GetDangerType())
      .WillRepeatedly(
          Return(download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING));
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  EXPECT_FALSE(info().ShouldShowDeepScanNotice());
}

TEST_F(DownloadBubbleRowViewInfoTest, ShouldNotShowIfScanAlreadyPerformed) {
  EXPECT_CALL(item(), GetDangerType())
      .WillRepeatedly(
          Return(download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING));
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingAutomaticDeepScanPerformed, true);
  EXPECT_FALSE(info().ShouldShowDeepScanNotice());
}

}  // namespace
