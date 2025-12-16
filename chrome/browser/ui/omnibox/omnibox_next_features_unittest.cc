// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_next_features.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/ntp_composebox_config.pb.h"
#include "ui/base/l10n/l10n_util.h"

namespace omnibox {

namespace {

std::string SerializeAndBase64EncodeProto(
    const google::protobuf::MessageLite& proto) {
  std::string serialized_proto;
  proto.SerializeToString(&serialized_proto);
  return base::Base64Encode(serialized_proto);
}

const char kConfigParamParseSuccessHistogram[] =
    "ContextualSearch.ConfigParseSuccess.Omnibox";

}  // namespace

class OmniboxNextFeaturesTest : public testing::Test {
 public:
  ScopedFeatureConfigForTesting scoped_config_;
};

// Tests the configuration when the feature is disabled.
TEST_F(OmniboxNextFeaturesTest, ComposeboxConfigDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(internal::kWebUIOmniboxAimPopup);

  base::HistogramTester histogram_tester;
  scoped_config_.Reset();

  omnibox::NTPComposeboxConfig config = scoped_config_.Get().config;
  EXPECT_EQ(config.entry_point().num_page_load_animations(), 3);

  histogram_tester.ExpectTotalCount(kConfigParamParseSuccessHistogram, 0);
}

// Tests the configuration when the feature is enabled with the
// default parameter.
TEST_F(OmniboxNextFeaturesTest, ComposeboxConfigEnabled_DefaultConfiguration) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(internal::kWebUIOmniboxAimPopup);

  base::HistogramTester histogram_tester;
  scoped_config_.Reset();

  omnibox::NTPComposeboxConfig config = scoped_config_.Get().config;
  EXPECT_EQ(config.entry_point().num_page_load_animations(), 3);

  auto composebox = config.composebox();
  EXPECT_TRUE(composebox.close_by_escape());
  EXPECT_TRUE(composebox.close_by_click_outside());

  auto image_upload = config.composebox().image_upload();
  EXPECT_EQ(image_upload.enable_webp_encoding(), false);
  EXPECT_EQ(image_upload.downscale_max_image_size(), 1500000);
  EXPECT_EQ(image_upload.downscale_max_image_width(), 1600);
  EXPECT_EQ(image_upload.downscale_max_image_height(), 1600);
  EXPECT_EQ(image_upload.image_compression_quality(), 40);
  EXPECT_THAT(image_upload.mime_types_allowed(),
              "image/avif,image/bmp,image/jpeg,image/png,image/webp,image/"
              "heif,image/heic");

  auto attachment_upload = config.composebox().attachment_upload();
  EXPECT_EQ(attachment_upload.max_size_bytes(), 200000000);
  EXPECT_THAT(attachment_upload.mime_types_allowed(), ".pdf,application/pdf");

  EXPECT_EQ(composebox.max_num_files(), 1);
  EXPECT_EQ(composebox.input_placeholder_text(),
            l10n_util::GetStringUTF8(IDS_NTP_COMPOSE_PLACEHOLDER_TEXT));
  EXPECT_EQ(composebox.is_pdf_upload_enabled(), true);

  auto placeholder_config = composebox.placeholder_config();
  EXPECT_EQ(placeholder_config.change_text_animation_interval_ms(), 4000u);
  EXPECT_EQ(placeholder_config.fade_text_animation_duration_ms(), 250u);
  EXPECT_EQ(placeholder_config.placeholders().size(), 6);

  histogram_tester.ExpectTotalCount(kConfigParamParseSuccessHistogram, 0);
}

// Tests the configuration when the feature is enabled with an
// invalid Base64-encoded proto parameter.
TEST_F(OmniboxNextFeaturesTest, ComposeboxConfigEnabled_Invalid_Configuration) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      internal::kWebUIOmniboxAimPopup, {{kConfigParam.name, "hello world"}});

  base::HistogramTester histogram_tester;
  scoped_config_.Reset();

  omnibox::NTPComposeboxConfig config = scoped_config_.Get().config;
  EXPECT_EQ(config.entry_point().num_page_load_animations(), 3);

  histogram_tester.ExpectTotalCount(kConfigParamParseSuccessHistogram, 1);
  histogram_tester.ExpectBucketCount(kConfigParamParseSuccessHistogram, false,
                                     1);
}

// Tests the configuration when the feature is enabled with a
// valid Base64-encoded proto parameter that does not set a value.
TEST_F(OmniboxNextFeaturesTest,
       ComposeboxConfigEnabled_Valid_Unset_Configuration) {
  omnibox::NTPComposeboxConfig fieldtrial_config;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      internal::kWebUIOmniboxAimPopup,
      {{kConfigParam.name, SerializeAndBase64EncodeProto(fieldtrial_config)}});

  base::HistogramTester histogram_tester;
  scoped_config_.Reset();

  omnibox::NTPComposeboxConfig config = scoped_config_.Get().config;
  EXPECT_EQ(config.entry_point().num_page_load_animations(), 3);

  histogram_tester.ExpectTotalCount(kConfigParamParseSuccessHistogram, 0);
}

// Tests the configuration when the feature is enabled with a
// valid Base64-encoded proto parameter that sets a value.
TEST_F(OmniboxNextFeaturesTest,
       ComposeboxConfigEnabled_Valid_Set_Configuration) {
  omnibox::NTPComposeboxConfig fieldtrial_config;
  fieldtrial_config.mutable_entry_point()->set_num_page_load_animations(5);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      internal::kWebUIOmniboxAimPopup,
      {{kConfigParam.name, SerializeAndBase64EncodeProto(fieldtrial_config)}});

  base::HistogramTester histogram_tester;
  scoped_config_.Reset();

  omnibox::NTPComposeboxConfig config = scoped_config_.Get().config;
  EXPECT_EQ(config.entry_point().num_page_load_animations(), 5);

  histogram_tester.ExpectTotalCount(kConfigParamParseSuccessHistogram, 1);
  histogram_tester.ExpectBucketCount(kConfigParamParseSuccessHistogram, true,
                                     1);
}

// Tests that setting `mime_types_allowed` for images in the `fieldtrial_config`
// overrides the default image mime types.
TEST_F(OmniboxNextFeaturesTest,
       ComposeboxConfigEnabled_Valid_OverrideImageMimeTypes) {
  omnibox::NTPComposeboxConfig fieldtrial_config;
  fieldtrial_config.mutable_composebox()
      ->mutable_image_upload()
      ->set_mime_types_allowed("image/png");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      internal::kWebUIOmniboxAimPopup,
      {{kConfigParam.name, SerializeAndBase64EncodeProto(fieldtrial_config)}});

  base::HistogramTester histogram_tester;
  scoped_config_.Reset();

  omnibox::NTPComposeboxConfig config = scoped_config_.Get().config;
  // Check that the image mime types were overridden.
  EXPECT_THAT(config.composebox().image_upload().mime_types_allowed(),
              "image/png");

  histogram_tester.ExpectUniqueSample(kConfigParamParseSuccessHistogram, true,
                                      1);
}

// Tests that setting `mime_types_allowed` for attachments in the
// `fieldtrial_config` overrides the default attachment mime types.
TEST_F(OmniboxNextFeaturesTest,
       ComposeboxConfigEnabled_Valid_OverrideAttachmentMimeTypes) {
  omnibox::NTPComposeboxConfig fieldtrial_config;
  fieldtrial_config.mutable_composebox()
      ->mutable_attachment_upload()
      ->set_mime_types_allowed("text/plain");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      internal::kWebUIOmniboxAimPopup,
      {{kConfigParam.name, SerializeAndBase64EncodeProto(fieldtrial_config)}});

  base::HistogramTester histogram_tester;
  scoped_config_.Reset();

  omnibox::NTPComposeboxConfig config = scoped_config_.Get().config;
  // Check that the attachment mime types were overridden.
  EXPECT_THAT(config.composebox().attachment_upload().mime_types_allowed(),
              "text/plain");

  histogram_tester.ExpectUniqueSample(kConfigParamParseSuccessHistogram, true,
                                      1);
}

// Tests that providing an empty `mime_types_allowed` value in the fieldtrial
// config does not clear the default value.
TEST_F(OmniboxNextFeaturesTest, ComposeboxConfigEnabled_Valid_ClearMimeTypes) {
  omnibox::NTPComposeboxConfig fieldtrial_config;
  // Providing an empty `mime_types_allowed`, will not clear the default
  // config.
  fieldtrial_config.mutable_composebox()
      ->mutable_image_upload()
      ->mime_types_allowed();
  fieldtrial_config.mutable_composebox()
      ->mutable_attachment_upload()
      ->mime_types_allowed();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      internal::kWebUIOmniboxAimPopup,
      {{kConfigParam.name, SerializeAndBase64EncodeProto(fieldtrial_config)}});

  base::HistogramTester histogram_tester;
  scoped_config_.Reset();

  omnibox::NTPComposeboxConfig config = scoped_config_.Get().config;
  // Check that both default mime type lists were cleared.
  EXPECT_THAT(config.composebox().image_upload().mime_types_allowed(),
              "image/avif,image/bmp,image/jpeg,image/png,image/webp,image/"
              "heif,image/heic");
  EXPECT_THAT(config.composebox().attachment_upload().mime_types_allowed(),
              ".pdf,application/pdf");

  histogram_tester.ExpectUniqueSample(kConfigParamParseSuccessHistogram, true,
                                      1);
}

class TestingAimEligibilityService : public ChromeAimEligibilityService {
 public:
  TestingAimEligibilityService(Profile* profile, bool is_aim_eligible)
      : ChromeAimEligibilityService(*profile->GetPrefs(),
                                    /*template_url_service=*/nullptr,
                                    /*url_loader_factory=*/nullptr,
                                    /*identity_manager=*/nullptr,
                                    /*is_off_the_record=*/false),
        is_aim_eligible_(is_aim_eligible) {}

  bool IsAimEligible() const override { return is_aim_eligible_; }

 private:
  const bool is_aim_eligible_;
};

class OmniboxNextAimEligibilityTest : public testing::Test {
 public:
  OmniboxNextAimEligibilityTest() = default;

 protected:
  void SetUpAimEligibilityService(bool is_aim_eligible) {
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindOnce(
                       [](bool is_eligible, content::BrowserContext* context)
                           -> std::unique_ptr<KeyedService> {
                         return std::make_unique<TestingAimEligibilityService>(
                             static_cast<TestingProfile*>(context),
                             is_eligible);
                       },
                       is_aim_eligible));
  }

  TestingProfile* profile() { return &profile_; }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(OmniboxNextAimEligibilityTest, IsAimPopupEnabled) {
  const struct {
    bool feature_enabled;
    bool is_aim_eligible;
    bool expected_popup_enabled;
  } test_cases[] = {
      {true, true, true},
      {true, false, false},
      {false, true, false},
      {false, false, false},
  };

  for (const auto& test_case : test_cases) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(internal::kWebUIOmniboxAimPopup,
                                      test_case.feature_enabled);

    SetUpAimEligibilityService(test_case.is_aim_eligible);

    EXPECT_EQ(omnibox::IsAimPopupEnabled(profile()),
              test_case.expected_popup_enabled);
  }
}

}  // namespace omnibox
