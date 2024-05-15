// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_security_view_info.h"

#include "base/strings/pattern.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/vector_icons.h"

using DownloadUIModelPtr = DownloadUIModel::DownloadUIModelPtr;
using TailoredVerdict = safe_browsing::ClientDownloadResponse::TailoredVerdict;
using download::DownloadItem;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

class DownloadBubbleSecurityViewInfoTest
    : public ::testing::Test,
      public DownloadBubbleSecurityViewInfoObserver {
 public:
  DownloadBubbleSecurityViewInfoTest() = default;

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
    info_ = std::make_unique<DownloadBubbleSecurityViewInfo>();
  }

  NiceMock<download::MockDownloadItem>& item() { return *item_; }
  DownloadBubbleSecurityViewInfo& info() { return *info_; }
  Profile* profile() { return &profile_; }
  TestingProfile& testing_profile() { return profile_; }

  void RefreshInfo() { info_->PopulateForDownload(item_.get()); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<NiceMock<download::MockDownloadItem>> item_;
  DownloadUIModelPtr model_;
  std::unique_ptr<DownloadBubbleSecurityViewInfo> info_;
};

// TODO: Remove the following test fixture once the ChromeRefresh flags are
//       removed or they're on by default.
class DownloadBubbleSecurityViewInfoTestGM3
    : public DownloadBubbleSecurityViewInfoTest {
 public:
  DownloadBubbleSecurityViewInfoTestGM3() = default;
  ~DownloadBubbleSecurityViewInfoTestGM3() override = default;

  void SetUp() override {
    DownloadBubbleSecurityViewInfoTest::SetUp();
    if (IsSkipped()) {
      return;
    }
  }
};

TEST_F(DownloadBubbleSecurityViewInfoTest, DangerousWarningInfo) {
  const struct DangerTypeTestCase {
    download::DownloadDangerType danger_type;
    std::optional<DownloadCommands::Command> primary_button_command;
    std::vector<DownloadCommands::Command> subpage_button_commands;
  } kDangerTypeTestCases[] = {
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
       std::nullopt,
       {DownloadCommands::Command::DISCARD, DownloadCommands::Command::KEEP}},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT,
       std::nullopt,
       {DownloadCommands::Command::DISCARD}},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST,
       std::nullopt,
       {DownloadCommands::Command::DISCARD}},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE,
       std::nullopt,
       {DownloadCommands::Command::DISCARD}},
      {download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED,
       std::nullopt,
       {DownloadCommands::Command::DISCARD}},
      {download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
       std::nullopt,
       {DownloadCommands::Command::DISCARD}},
      {download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING,
       DownloadCommands::Command::DISCARD,
       {DownloadCommands::Command::DISCARD, DownloadCommands::Command::KEEP}},
      {download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT,
       std::nullopt,
       {DownloadCommands::Command::DISCARD, DownloadCommands::Command::KEEP}},
      {download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING,
       std::nullopt,
       {DownloadCommands::Command::DEEP_SCAN,
        DownloadCommands::Command::BYPASS_DEEP_SCANNING}},
      {download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING,
       std::nullopt,
       {DownloadCommands::Command::DISCARD,
        DownloadCommands::Command::CANCEL_DEEP_SCAN}},
  };
  for (const auto& test_case : kDangerTypeTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Failed for danger type "
                 << download::GetDownloadDangerTypeString(test_case.danger_type)
                 << std::endl);
    ON_CALL(item(), GetDangerType())
        .WillByDefault(Return(test_case.danger_type));
    RefreshInfo();
    std::vector<DownloadCommands::Command> subpage_commands;
    if (info().has_primary_button()) {
      subpage_commands.push_back(info().primary_button().command);
    }
    if (info().has_secondary_button()) {
      subpage_commands.push_back(info().secondary_button().command);
    }
    EXPECT_EQ(subpage_commands, test_case.subpage_button_commands);
  }
}

TEST_F(DownloadBubbleSecurityViewInfoTestGM3, InterruptedInfo) {
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
    std::vector<download::DownloadInterruptReason> interrupt_reasons;
    bool can_resume;

    std::string expected_warning_summary;
    raw_ptr<const gfx::VectorIcon> expected_icon_model_override;
    std::optional<DownloadCommands::Command> expected_primary_button_command;
  } kTestCases[] = {
      {{download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED},
       false,
       "Your organization blocked this file because it didn't meet a security "
       "policy",
       &views::kInfoChromeRefreshIcon,
       std::optional<DownloadCommands::Command>()},
      {{download::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG},
       false,
       "Try using a shorter file name or saving to a different folder",
       &vector_icons::kFileDownloadOffChromeRefreshIcon,
       std::optional<DownloadCommands::Command>()},
      {{download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE},
       false,
       "Free up space on your device. Then, try to download again",
       &vector_icons::kFileDownloadOffChromeRefreshIcon,
       std::optional<DownloadCommands::Command>()},
      {{download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED},
       false,
       "Try to sign in to the site. Then, download again",
       &vector_icons::kFileDownloadOffChromeRefreshIcon,
       std::optional<DownloadCommands::Command>()},
      {no_retry_interrupt_reasons, false, "",
       &vector_icons::kFileDownloadOffChromeRefreshIcon,
       std::optional<DownloadCommands::Command>()},
      {retry_interrupt_reasons, false, "",
       &vector_icons::kFileDownloadOffChromeRefreshIcon,
       DownloadCommands::Command::RETRY},
      {retry_interrupt_reasons, true, "",
       &vector_icons::kFileDownloadOffChromeRefreshIcon,
       DownloadCommands::Command::RESUME},
  };

  for (const auto& test_case : kTestCases) {
    for (const auto& interrupt_reason : test_case.interrupt_reasons) {
      EXPECT_CALL(item(), GetLastReason())
          .WillRepeatedly(Return(interrupt_reason));
      EXPECT_CALL(item(), GetState())
          .WillRepeatedly(Return(
              (interrupt_reason == download::DOWNLOAD_INTERRUPT_REASON_NONE)
                  ? DownloadItem::IN_PROGRESS
                  : DownloadItem::INTERRUPTED));
      EXPECT_CALL(item(), CanResume())
          .WillRepeatedly(Return(test_case.can_resume));
      RefreshInfo();

      EXPECT_EQ(test_case.expected_warning_summary,
                base::UTF16ToUTF8(info().warning_summary()));
      EXPECT_EQ(test_case.expected_icon_model_override,
                info().icon_model_override());
      EXPECT_EQ(kColorDownloadItemIconDangerous, info().secondary_color());
      EXPECT_FALSE(info().has_progress_bar());
    }
  }
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
// Test file type warning where verdict was obtained.
TEST_F(DownloadBubbleSecurityViewInfoTest,
       FileTypeWarning_HasSafeBrowsingVerdict) {
  EXPECT_CALL(item(), GetState())
      .WillRepeatedly(Return(DownloadItem::IN_PROGRESS));

  for (auto sb_state : {safe_browsing::SafeBrowsingState::STANDARD_PROTECTION,
                        safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION,
                        // This can happen if the user subsequently turned off
                        // SB after verdict was obtained.
                        safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING}) {
    SetSafeBrowsingState(profile()->GetPrefs(), sb_state);
    EXPECT_CALL(item(), GetDangerType())
        .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
    safe_browsing::DownloadProtectionService::SetDownloadProtectionData(
        &item(), "token", safe_browsing::ClientDownloadResponse::DANGEROUS,
        safe_browsing::ClientDownloadResponse::TailoredVerdict());
    RefreshInfo();

    // Subpage warning
    EXPECT_TRUE(info().HasSubpage());
    EXPECT_TRUE(base::MatchPattern(info().warning_summary(),
                                   u"*file type isn't commonly downloaded*"));
    // Primary subpage button.
    ASSERT_TRUE(info().has_primary_button());
    EXPECT_EQ(info().primary_button().command, DownloadCommands::DISCARD);
    EXPECT_EQ(info().primary_button().label, u"Delete from history");
    // Secondary subpage button. File is described as "suspicious".
    EXPECT_EQ(info().secondary_button().command, DownloadCommands::KEEP);
    EXPECT_EQ(info().secondary_button().label, u"Download suspicious file");
    // Learn more link.
    EXPECT_TRUE(info().learn_more_link().has_value());
    EXPECT_TRUE(
        base::MatchPattern(info().learn_more_link()->label_and_link_text,
                           u"Learn why * blocks some downloads"));
  }
}

// Test file type warning where SB is on but no SB verdict was obtained.
TEST_F(DownloadBubbleSecurityViewInfoTest,
       FileTypeWarning_SafeBrowsingOn_NoVerdict) {
  EXPECT_CALL(item(), GetState())
      .WillRepeatedly(Return(DownloadItem::IN_PROGRESS));
  for (auto sb_state :
       {safe_browsing::SafeBrowsingState::STANDARD_PROTECTION,
        safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION}) {
    SetSafeBrowsingState(profile()->GetPrefs(), sb_state);
    EXPECT_CALL(item(), GetDangerType())
        .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));

    RefreshInfo();

    // Subpage warning
    EXPECT_TRUE(info().HasSubpage());
    EXPECT_TRUE(base::MatchPattern(info().warning_summary(),
                                   u"*file type isn't commonly downloaded*"));
    // Suspicious pattern has 2 buttons on the subpage.
    // Primary subpage button.
    ASSERT_TRUE(info().has_primary_button());
    EXPECT_EQ(info().primary_button().command, DownloadCommands::DISCARD);
    EXPECT_EQ(info().primary_button().label, u"Delete from history");
    // Secondary subpage button. File is described as "unverified".
    ASSERT_TRUE(info().has_secondary_button());
    EXPECT_EQ(info().secondary_button().command, DownloadCommands::KEEP);
    EXPECT_EQ(info().secondary_button().label, u"Download unverified file");
    // Learn more link.
    EXPECT_TRUE(info().learn_more_link().has_value());
    EXPECT_TRUE(
        base::MatchPattern(info().learn_more_link()->label_and_link_text,
                           u"Learn why * blocks some downloads"));
  }
}

// Test file type warning where SB is disabled by pref and can be turned on.
TEST_F(DownloadBubbleSecurityViewInfoTest,
       FileTypeWarning_NoSafeBrowsing_DisabledByPref) {
  EXPECT_CALL(item(), GetState())
      .WillRepeatedly(Return(DownloadItem::IN_PROGRESS));
  SetSafeBrowsingState(profile()->GetPrefs(),
                       safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);
  EXPECT_CALL(item(), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));

  RefreshInfo();

  // Subpage warning
  EXPECT_TRUE(info().HasSubpage());
  EXPECT_TRUE(base::MatchPattern(info().warning_summary(),
                                 u"*file can't be verified*"));
  // Suspicious pattern has 2 buttons on the subpage.
  // Primary subpage button.
  ASSERT_TRUE(info().has_primary_button());
  EXPECT_EQ(info().primary_button().command, DownloadCommands::DISCARD);
  EXPECT_EQ(info().primary_button().label, u"Delete from history");
  // Secondary subpage button. File is described as "unverified".
  ASSERT_TRUE(info().has_secondary_button());
  EXPECT_EQ(info().secondary_button().command, DownloadCommands::KEEP);
  EXPECT_EQ(info().secondary_button().label, u"Download unverified file");
  // Learn more link.
  EXPECT_TRUE(info().learn_more_link().has_value());
  EXPECT_TRUE(base::MatchPattern(info().learn_more_link()->label_and_link_text,
                                 u"Turn on Safe Browsing to *"));
}

// Test file type warning where SB is disabled by enterprise controls and cannot
// be turned on.
TEST_F(DownloadBubbleSecurityViewInfoTest,
       FileTypeWarning_NoSafeBrowsing_DisabledManaged) {
  EXPECT_CALL(item(), GetState())
      .WillRepeatedly(Return(DownloadItem::IN_PROGRESS));
  SetSafeBrowsingState(profile()->GetPrefs(),
                       safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);
  testing_profile().GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(false));
  EXPECT_CALL(item(), GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));

  RefreshInfo();

  // Subpage warning
  EXPECT_TRUE(info().HasSubpage());
  EXPECT_TRUE(base::MatchPattern(info().warning_summary(),
                                 u"*file can't be verified*"));
  // Suspicious pattern has 2 buttons on the subpage.
  // Primary subpage button.
  ASSERT_TRUE(info().has_primary_button());
  EXPECT_EQ(info().primary_button().command, DownloadCommands::DISCARD);
  EXPECT_EQ(info().primary_button().label, u"Delete from history");
  // Secondary subpage button. File is described as "unverified".
  ASSERT_TRUE(info().has_secondary_button());
  EXPECT_EQ(info().secondary_button().command, DownloadCommands::KEEP);
  EXPECT_EQ(info().secondary_button().label, u"Download unverified file");
  // There is no learn more link because the user cannot turn on SB.
  EXPECT_FALSE(info().learn_more_link().has_value());
}
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
class DownloadBubbleSecurityViewInfoTailoredWarningTest
    : public DownloadBubbleSecurityViewInfoTest {
 public:
  DownloadBubbleSecurityViewInfoTailoredWarningTest() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    scoped_feature_list_.InitAndEnableFeature(
        safe_browsing::kDownloadTailoredWarnings);
  }

  ~DownloadBubbleSecurityViewInfoTailoredWarningTest() override = default;

 protected:
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
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DownloadBubbleSecurityViewInfoTailoredWarningTest,
       GetInfoForTailoredWarning_CookieTheft) {
  SetupTailoredWarningForItem(
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE,
      TailoredVerdict::COOKIE_THEFT, /*adjustments=*/{});
  RefreshInfo();

  ASSERT_TRUE(info().has_primary_button());
  EXPECT_FALSE(info().has_secondary_button());
  EXPECT_EQ(info().primary_button().command,
            DownloadCommands::Command::DISCARD);
  EXPECT_TRUE(info().primary_button().is_prominent);
  EXPECT_EQ(info().warning_summary(),
            u"This file can harm your personal and social network accounts");
}

TEST_F(DownloadBubbleSecurityViewInfoTailoredWarningTest,
       GetInfoForTailoredWarning_SuspiciousArchive) {
  SetupTailoredWarningForItem(download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT,
                              TailoredVerdict::SUSPICIOUS_ARCHIVE,
                              /*adjustments=*/{});
  RefreshInfo();

  ASSERT_TRUE(info().has_primary_button());
  EXPECT_EQ(info().primary_button().command,
            DownloadCommands::Command::DISCARD);
  EXPECT_TRUE(info().primary_button().is_prominent);

  ASSERT_TRUE(info().has_secondary_button());
  EXPECT_EQ(info().secondary_button().command, DownloadCommands::Command::KEEP);
  EXPECT_FALSE(info().secondary_button().is_prominent);
  EXPECT_EQ(info().warning_summary(),
            u"This archive file includes other files that may hide malware");
}

TEST_F(DownloadBubbleSecurityViewInfoTailoredWarningTest,
       GetInfoForTailoredWarning_AccountInfoStringWithAccount) {
  SetupTailoredWarningForItem(
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE,
      TailoredVerdict::COOKIE_THEFT, {TailoredVerdict::ACCOUNT_INFO_STRING});
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  signin::SetPrimaryAccount(identity_manager, "test@example.com",
                            signin::ConsentLevel::kSignin);
  RefreshInfo();

  ASSERT_TRUE(info().has_primary_button());
  EXPECT_FALSE(info().has_secondary_button());
  EXPECT_EQ(info().primary_button().command,
            DownloadCommands::Command::DISCARD);
  EXPECT_TRUE(info().primary_button().is_prominent);
  EXPECT_EQ(info().warning_summary(),
            u"This file can harm your personal and social network accounts, "
            u"including test@example.com");
}

TEST_F(DownloadBubbleSecurityViewInfoTailoredWarningTest,
       GetInfoForTailoredWarning_AccountInfoStringWithoutAccount) {
  SetupTailoredWarningForItem(
      download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE,
      TailoredVerdict::COOKIE_THEFT, {TailoredVerdict::ACCOUNT_INFO_STRING});
  RefreshInfo();

  ASSERT_TRUE(info().has_primary_button());
  EXPECT_FALSE(info().has_secondary_button());
  EXPECT_EQ(info().primary_button().command,
            DownloadCommands::Command::DISCARD);
  EXPECT_TRUE(info().primary_button().is_prominent);
  EXPECT_EQ(info().warning_summary(),
            u"This file can harm your personal and social network accounts");
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
