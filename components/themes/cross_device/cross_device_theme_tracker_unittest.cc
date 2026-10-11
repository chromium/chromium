// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/cross_device/cross_device_theme_tracker.h"

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/sync/protocol/theme_android_specifics.pb.h"
#include "components/sync/protocol/theme_ios_specifics.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/themes/common/image_url_options.h"
#include "components/themes/cross_device/theme_comparer.h"
#include "components/themes/cross_device/theme_translation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace themes {

namespace {

class MockObserver
    : public CrossDeviceThemeTracker<sync_pb::ThemeSpecifics>::Observer {
 public:
  MOCK_METHOD(void, OnCrossDeviceThemeChanged, (), (override));
  MOCK_METHOD(void, OnServiceStatusChanged, (ServiceStatus), (override));
};

// Testable subclass to expose protected methods.
class TestCrossDeviceThemeTracker
    : public CrossDeviceThemeTracker<sync_pb::ThemeSpecifics> {
 public:
  TestCrossDeviceThemeTracker() = default;

  using CrossDeviceThemeTracker::RemoveThemeInfo;
  using CrossDeviceThemeTracker::SetStatus;
  using CrossDeviceThemeTracker::UpdateThemeInfo;

  void SetBridgeStatusForTesting(syncer::DataType type, ServiceStatus status) {
    this->bridge_statuses_[type] = status;
    this->UpdateAggregateStatus();
  }
};

class CrossDeviceThemeTrackerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  TestCrossDeviceThemeTracker tracker_;
};

TEST_F(CrossDeviceThemeTrackerTest, InitialState) {
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kInitializing);
  EXPECT_TRUE(tracker_.GetOtherDevicesThemes().empty());
}

TEST_F(CrossDeviceThemeTrackerTest, UpdateAndRemoveTheme) {
  MockObserver observer;
  tracker_.AddObserver(&observer);

  DeviceThemeInfo<sync_pb::ThemeSpecifics> theme_info;
  theme_info.data_type = syncer::THEMES;
  theme_info.theme.mutable_user_color_theme()->set_color(SK_ColorBLUE);

  // Expect observer notification on update.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.UpdateThemeInfo("current_theme", theme_info);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Verify theme is in the list.
  auto themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].data_type, syncer::THEMES);
  ASSERT_TRUE(themes[0].theme.has_user_color_theme());
  EXPECT_EQ(themes[0].theme.user_color_theme().color(), SK_ColorBLUE);

  // Update same tag.
  theme_info.theme.mutable_user_color_theme()->set_color(SK_ColorRED);
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.UpdateThemeInfo("current_theme", theme_info);
  testing::Mock::VerifyAndClearExpectations(&observer);

  themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  ASSERT_TRUE(themes[0].theme.has_user_color_theme());
  EXPECT_EQ(themes[0].theme.user_color_theme().color(), SK_ColorRED);

  // Update with same info, expect NO notification.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(0);
  tracker_.UpdateThemeInfo("current_theme", theme_info);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Add another tag.
  DeviceThemeInfo<sync_pb::ThemeSpecifics> theme_info2;
  theme_info2.data_type = syncer::THEMES_IOS;
  theme_info2.theme.mutable_user_color_theme()->set_color(SK_ColorGREEN);

  // Expect observer notification on update.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.UpdateThemeInfo("current_theme_ios", theme_info2);
  testing::Mock::VerifyAndClearExpectations(&observer);

  themes = tracker_.GetOtherDevicesThemes();
  EXPECT_EQ(themes.size(), 2u);

  // Remove one.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.RemoveThemeInfo("current_theme");
  testing::Mock::VerifyAndClearExpectations(&observer);

  themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].data_type, syncer::THEMES_IOS);

  // Remove non-existent.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(0);
  tracker_.RemoveThemeInfo("tag_non_existent");
  testing::Mock::VerifyAndClearExpectations(&observer);

  tracker_.RemoveObserver(&observer);
}

TEST_F(CrossDeviceThemeTrackerTest, StatusChanges) {
  MockObserver observer;
  tracker_.AddObserver(&observer);

  EXPECT_CALL(observer, OnServiceStatusChanged(ServiceStatus::kActive))
      .Times(1);
  tracker_.SetStatus(ServiceStatus::kActive);
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kActive);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Set same status, no notification.
  EXPECT_CALL(observer, OnServiceStatusChanged(testing::_)).Times(0);
  tracker_.SetStatus(ServiceStatus::kActive);
  testing::Mock::VerifyAndClearExpectations(&observer);

  tracker_.RemoveObserver(&observer);
}

TEST_F(CrossDeviceThemeTrackerTest, BridgeStatusAggregation) {
  MockObserver observer;
  tracker_.AddObserver(&observer);

  // Initial status is kInitializing.
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kInitializing);

  // One bridge becomes active -> kActive.
  EXPECT_CALL(observer, OnServiceStatusChanged(ServiceStatus::kActive))
      .Times(1);
  tracker_.SetBridgeStatusForTesting(syncer::THEMES_ANDROID,
                                     ServiceStatus::kActive);
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kActive);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Other bridge becomes disabled -> still kActive (since Android is active).
  EXPECT_CALL(observer, OnServiceStatusChanged(testing::_)).Times(0);
  tracker_.SetBridgeStatusForTesting(syncer::THEMES_IOS,
                                     ServiceStatus::kSyncDisabled);
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kActive);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Android also becomes disabled -> all disabled -> kSyncDisabled.
  EXPECT_CALL(observer, OnServiceStatusChanged(ServiceStatus::kSyncDisabled))
      .Times(1);
  tracker_.SetBridgeStatusForTesting(syncer::THEMES_ANDROID,
                                     ServiceStatus::kSyncDisabled);
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kSyncDisabled);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Android becomes initializing -> kInitializing.
  EXPECT_CALL(observer, OnServiceStatusChanged(ServiceStatus::kInitializing))
      .Times(1);
  tracker_.SetBridgeStatusForTesting(syncer::THEMES_ANDROID,
                                     ServiceStatus::kInitializing);
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kInitializing);
  testing::Mock::VerifyAndClearExpectations(&observer);

  tracker_.RemoveObserver(&observer);
}

TEST_F(CrossDeviceThemeTrackerTest, OsTypeToDataType) {
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kAndroid,
                             syncer::DeviceInfo::FormFactor::kPhone),
            syncer::THEMES_ANDROID);
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kAndroid,
                             syncer::DeviceInfo::FormFactor::kDesktop),
            syncer::THEMES);
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kIOS,
                             syncer::DeviceInfo::FormFactor::kPhone),
            syncer::THEMES_IOS);
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kWindows,
                             syncer::DeviceInfo::FormFactor::kDesktop),
            syncer::THEMES);
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kMac,
                             syncer::DeviceInfo::FormFactor::kDesktop),
            syncer::THEMES);
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kLinux,
                             syncer::DeviceInfo::FormFactor::kDesktop),
            syncer::THEMES);
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kChromeOsAsh,
                             syncer::DeviceInfo::FormFactor::kDesktop),
            syncer::THEMES);
  EXPECT_EQ(OsTypeToDataType(syncer::DeviceInfo::OsType::kUnknown,
                             syncer::DeviceInfo::FormFactor::kUnknown),
            syncer::UNSPECIFIED);
}

TEST_F(CrossDeviceThemeTrackerTest,
       OnBridgeSyncDisabledClearsThemesForDataType) {
  tracker_.OnBridgeSyncStarted(syncer::THEMES);
  tracker_.OnBridgeSyncStarted(syncer::THEMES_IOS);
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kActive);

  DeviceThemeInfo<sync_pb::ThemeSpecifics> desktop_theme;
  desktop_theme.data_type = syncer::THEMES;
  desktop_theme.theme.mutable_user_color_theme()->set_color(SK_ColorBLUE);
  tracker_.UpdateThemeInfo("current_theme", desktop_theme);

  DeviceThemeInfo<sync_pb::ThemeSpecifics> ios_theme;
  ios_theme.data_type = syncer::THEMES_IOS;
  ios_theme.theme.mutable_user_color_theme()->set_color(SK_ColorGREEN);
  tracker_.UpdateThemeInfo("current_theme_ios", ios_theme);

  ASSERT_EQ(tracker_.GetOtherDevicesThemes().size(), 2u);

  tracker_.OnBridgeSyncDisabled(syncer::THEMES);

  auto themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].data_type, syncer::THEMES_IOS);
}

class ThemeTranslationTest : public testing::Test {
 protected:
  static constexpr char kTestCollectionId[] = "collection_1";
  static constexpr char kBaseImageUrl[] =
      "https://lh3.googleusercontent.com/proxy/image123";
  static constexpr char kMobileImageOptions[] = "=s2556-k-no-nd";

#if BUILDFLAG(IS_ANDROID)
  static constexpr char kDesktopImageOptions[] = "=w3840-h2160-p-k-no-nd-mv";

  using SourceSpecifics = sync_pb::ThemeSpecifics;
  using LocalSpecifics = sync_pb::ThemeAndroidSpecifics;

  DeviceThemeInfo<LocalSpecifics> TranslateSource(
      const SourceSpecifics& specifics) {
    return TranslateDesktop(specifics);
  }
#else
  using SourceSpecifics = sync_pb::ThemeAndroidSpecifics;
  using LocalSpecifics = sync_pb::ThemeSpecifics;

  DeviceThemeInfo<LocalSpecifics> TranslateSource(
      const SourceSpecifics& specifics) {
    return TranslateAndroid(specifics);
  }
#endif

  template <typename Specifics = SourceSpecifics>
  Specifics CreateSpecificsWithNtpBackground(
      std::optional<std::string> url = std::nullopt,
      std::optional<std::string> collection_id = kTestCollectionId) {
    Specifics specifics;
    auto* bg = specifics.mutable_ntp_background();
    if (collection_id.has_value()) {
      bg->set_collection_id(*collection_id);
    }
    if (url.has_value()) {
      bg->set_url(*url);
    }
    return specifics;
  }

  void VerifyTranslatedNtpBackground(
      const DeviceThemeInfo<LocalSpecifics>& translated,
      std::optional<std::string> expected_url,
      std::optional<std::string> expected_collection_id = kTestCollectionId) {
    ASSERT_TRUE(translated.theme.has_ntp_background());
    if (expected_url.has_value()) {
      EXPECT_EQ(translated.theme.ntp_background().url(), *expected_url);
    } else {
      EXPECT_FALSE(translated.theme.ntp_background().has_url());
    }
    if (expected_collection_id.has_value()) {
      EXPECT_EQ(translated.theme.ntp_background().collection_id(),
                *expected_collection_id);
    } else {
      EXPECT_FALSE(translated.theme.ntp_background().has_collection_id());
    }
  }
};

#if BUILDFLAG(IS_ANDROID)
TEST_F(ThemeTranslationTest, TranslateDesktopAndIosNormalizeNtpBackgroundUrl) {
  const std::string expected_url =
      std::string(kBaseImageUrl) + GetImageOptions();

  DeviceThemeInfo<sync_pb::ThemeAndroidSpecifics> translated_desktop =
      TranslateDesktop(
          CreateSpecificsWithNtpBackground<sync_pb::ThemeSpecifics>(
              std::string(kBaseImageUrl) + kDesktopImageOptions));
  EXPECT_TRUE(translated_desktop.theme.use_custom_theme());
  VerifyTranslatedNtpBackground(translated_desktop, expected_url);

  DeviceThemeInfo<sync_pb::ThemeAndroidSpecifics> translated_ios =
      TranslateIos(CreateSpecificsWithNtpBackground<sync_pb::ThemeIosSpecifics>(
          std::string(kBaseImageUrl) + kMobileImageOptions));
  EXPECT_TRUE(translated_ios.theme.use_custom_theme());
  VerifyTranslatedNtpBackground(translated_ios, expected_url);
  EXPECT_TRUE(ThemeComparer<sync_pb::ThemeAndroidSpecifics>::Equals(
      translated_desktop.theme, translated_ios.theme));
}
#else
TEST_F(ThemeTranslationTest, TranslateAndroidAndIosNormalizeNtpBackgroundUrl) {
  const std::string mobile_image_url =
      std::string(kBaseImageUrl) + kMobileImageOptions;
  const std::string expected_url =
      std::string(kBaseImageUrl) + GetImageOptions();

  DeviceThemeInfo<sync_pb::ThemeSpecifics> translated_android =
      TranslateAndroid(
          CreateSpecificsWithNtpBackground<sync_pb::ThemeAndroidSpecifics>(
              mobile_image_url));
  VerifyTranslatedNtpBackground(translated_android, expected_url);

  DeviceThemeInfo<sync_pb::ThemeSpecifics> translated_ios =
      TranslateIos(CreateSpecificsWithNtpBackground<sync_pb::ThemeIosSpecifics>(
          mobile_image_url));
  VerifyTranslatedNtpBackground(translated_ios, expected_url);
  EXPECT_TRUE(ThemeComparer<sync_pb::ThemeSpecifics>::Equals(
      translated_android.theme, translated_ios.theme));
}
#endif

TEST_F(ThemeTranslationTest, TranslateHandlesEmptyOrUnsetNtpBackgroundUrl) {
  VerifyTranslatedNtpBackground(
      TranslateSource(CreateSpecificsWithNtpBackground()),
      /*expected_url=*/std::nullopt);
  VerifyTranslatedNtpBackground(
      TranslateSource(CreateSpecificsWithNtpBackground("")),
      /*expected_url=*/"");
}

TEST_F(ThemeTranslationTest,
       TranslateLeavesNonHttpOrInvalidNtpBackgroundUrlUnchanged) {
  const std::string non_http_url = "chrome-search://local-ntp/background.jpg";
  VerifyTranslatedNtpBackground(
      TranslateSource(CreateSpecificsWithNtpBackground(non_http_url)),
      non_http_url);

  // Malformed sync data from a remote client must be left untouched and must
  // not trigger `GURL::spec()` on an invalid URL.
  const std::string invalid_url = "not a valid url=s2556-k-no-nd";
  VerifyTranslatedNtpBackground(
      TranslateSource(CreateSpecificsWithNtpBackground(invalid_url)),
      invalid_url);
}

TEST_F(ThemeTranslationTest,
       TranslateLeavesNtpBackgroundUrlWithoutCollectionIdUnchanged) {
  VerifyTranslatedNtpBackground(
      TranslateSource(CreateSpecificsWithNtpBackground(
          kBaseImageUrl, /*collection_id=*/std::nullopt)),
      kBaseImageUrl, /*expected_collection_id=*/std::nullopt);
  VerifyTranslatedNtpBackground(
      TranslateSource(CreateSpecificsWithNtpBackground(kBaseImageUrl,
                                                       /*collection_id=*/"")),
      kBaseImageUrl, /*expected_collection_id=*/"");
}

}  // namespace

}  // namespace themes
