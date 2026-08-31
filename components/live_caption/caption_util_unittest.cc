// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/caption_util.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/test/scoped_os_info_override_win.h"
#include "base/win/windows_version.h"
#endif

namespace captions {
class CaptionUtilTest : public testing::Test {
 public:
  CaptionUtilTest() = default;
  ~CaptionUtilTest() override = default;
  void SetUp() override {
    testing::Test::SetUp();
    SetTestPrefs(&pref_service_);
  }

  void SetTestPrefs(TestingPrefServiceSimple* pref_service) {
    pref_service->registry()->RegisterStringPref(
        prefs::kAccessibilityCaptionsTextSize, "");
    pref_service->registry()->RegisterStringPref(
        prefs::kAccessibilityCaptionsTextFont, "");
    pref_service->registry()->RegisterStringPref(
        prefs::kAccessibilityCaptionsTextColor, "");
    pref_service->registry()->RegisterIntegerPref(
        prefs::kAccessibilityCaptionsTextOpacity, 50);
    pref_service->registry()->RegisterStringPref(
        prefs::kAccessibilityCaptionsBackgroundColor, "");
    pref_service->registry()->RegisterIntegerPref(
        prefs::kAccessibilityCaptionsBackgroundOpacity, 50);
    pref_service->registry()->RegisterStringPref(
        prefs::kAccessibilityCaptionsTextShadow, "");
    pref_service->SetString(prefs::kAccessibilityCaptionsTextColor, "red");
    pref_service->SetString(prefs::kAccessibilityCaptionsBackgroundColor,
                            "green");
  }

  TestingPrefServiceSimple pref_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CaptionUtilTest, CommandLineOverride) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kForceCaptionStyle,
      "{\"text-color\":\"pink\",\"background-color\":\"blue\"}");
  std::optional<ui::CaptionStyle> style =
      GetCaptionStyleFromUserSettings(&pref_service_, /*record_metrics=*/false);

  ASSERT_TRUE(style.has_value());
  EXPECT_THAT(style->text_color, testing::StartsWith("pink"));
  EXPECT_THAT(style->background_color, testing::StartsWith("blue"));

  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ::switches::kForceCaptionStyle);
}

TEST_F(CaptionUtilTest, IsHeadlessCaptionFeatureSupportedReturnsFalse) {
  scoped_feature_list_.InitAndDisableFeature(media::kHeadlessLiveCaption);
  EXPECT_FALSE(IsHeadlessCaptionFeatureSupported());
}

#if BUILDFLAG(IS_FUCHSIA)
TEST_F(CaptionUtilTest,
       IsHeadlessCaptionFeatureSupportedReturnsFalseOnFuchsia) {
  scoped_feature_list_.InitAndEnableFeature(media::kHeadlessLiveCaption);
  EXPECT_FALSE(IsHeadlessCaptionFeatureSupported());
}
#elif BUILDFLAG(IS_LINUX) && !defined(ARCH_CPU_X86_FAMILY)
TEST_F(CaptionUtilTest,
       IsHeadlessCaptionFeatureSupportedReturnsFalseOnLinuxArm) {
  scoped_feature_list_.InitAndEnableFeature(media::kHeadlessLiveCaption);
  EXPECT_FALSE(IsHeadlessCaptionFeatureSupported());
}
#else
TEST_F(CaptionUtilTest, IsHeadlessCaptionFeatureSupportedReturnsTrue) {
#if BUILDFLAG(IS_CHROMEOS)
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{media::kHeadlessLiveCaption,
                            ash::features::kOnDeviceSpeechRecognition},
      /*disabled_features=*/{});
#else
  scoped_feature_list_.InitAndEnableFeature(media::kHeadlessLiveCaption);
#endif
  EXPECT_TRUE(IsHeadlessCaptionFeatureSupported());
}
#endif

TEST_F(CaptionUtilTest,
       IsHeadlessCaptionFeatureSupportedWithSmallExpertModelReturnsTrue) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{media::kHeadlessLiveCaption,
                            media::
                                kLiveCaptionSpeechRecognitionSmallExpertModel},
      /*disabled_features=*/{});
  EXPECT_TRUE(IsHeadlessCaptionFeatureSupported());
}

TEST_F(CaptionUtilTest, ReturnsCorrectCaptionSettingsUrl) {
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(GetCaptionSettingsUrl(), "chrome://os-settings/audioAndCaptions");
#elif BUILDFLAG(IS_LINUX)
  EXPECT_EQ(GetCaptionSettingsUrl(), "chrome://settings/captions");
#elif BUILDFLAG(IS_WIN)
  {
    base::test::ScopedOSInfoOverride os_override(
        base::test::ScopedOSInfoOverride::Type::kWin10Pro);
    EXPECT_EQ(GetCaptionSettingsUrl(), "chrome://settings/accessibility");
  }
#elif BUILDFLAG(IS_MAC)
  EXPECT_EQ(GetCaptionSettingsUrl(), "chrome://settings/accessibility");
#endif  // BUILDFLAG(IS_LINUX)
}

}  // namespace captions
