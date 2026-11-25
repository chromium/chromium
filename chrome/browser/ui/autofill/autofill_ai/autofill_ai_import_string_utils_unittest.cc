// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_string_utils.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/strings/utf_string_conversions.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {
namespace {

#if BUILDFLAG(IS_ANDROID)
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Not;
#endif  // BUILDFLAG(IS_ANDROID)

// Tests that GetPromptTitle returns unbranded strings when is_server_wallet is false,
// when is_banner_prompt is true, or when kAutofillAiWalletPassBranding2026 feature is disabled.
TEST(AutofillAiImportStringUtilsTest, GetPromptTitleUnbranded) {
  EXPECT_EQ(
      GetPromptTitle(EntityTypeName::kPassport, /*is_save_prompt=*/true,
                     /*is_banner_prompt=*/false, /*is_server_wallet=*/false),
      l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_ANDROID)
          IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID
#else
          IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE
#endif
          ));

  EXPECT_EQ(
      GetPromptTitle(EntityTypeName::kPassport, /*is_save_prompt=*/false,
                     /*is_banner_prompt=*/false, /*is_server_wallet=*/false),
      l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_ANDROID)
          IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID
#else
          IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE
#endif
          ));

  // Banner prompts disable branding even when server wallet is true and feature is enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kAutofillAiWalletPassBranding2026);
    EXPECT_EQ(
        GetPromptTitle(EntityTypeName::kPassport, /*is_save_prompt=*/true,
                       /*is_banner_prompt=*/true, /*is_server_wallet=*/true),
        l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_ANDROID)
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID
#else
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE
#endif
            ));
  }
}

// Tests that GetPromptTitle respects branding options and variants when enabled.
TEST(AutofillAiImportStringUtilsTest, GetPromptTitleBrandedVariants) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kAutofillAiWalletPassBranding2026);

    // Default variant 0 (save and update prompts)
    EXPECT_EQ(
        GetPromptTitle(EntityTypeName::kPassport, /*is_save_prompt=*/true,
                       /*is_banner_prompt=*/false, /*is_server_wallet=*/true),
        l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_ANDROID)
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID_BRANDED
#else
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_BRANDED
#endif
            ));
    EXPECT_EQ(
        GetPromptTitle(EntityTypeName::kPassport, /*is_save_prompt=*/false,
                       /*is_banner_prompt=*/false, /*is_server_wallet=*/true),
        l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_ANDROID)
            IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID_BRANDED
#else
            IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_BRANDED
#endif
            ));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kAutofillAiWalletPassBranding2026, {{"string_variant", "1"}});

    // Variant 1 (save and update prompts)
    EXPECT_EQ(
        GetPromptTitle(EntityTypeName::kPassport, /*is_save_prompt=*/true,
                       /*is_banner_prompt=*/false, /*is_server_wallet=*/true),
        l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_ANDROID)
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_1_BRANDED
#else
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED
#endif
            ));

    EXPECT_EQ(
        GetPromptTitle(EntityTypeName::kPassport, /*is_save_prompt=*/false,
                       /*is_banner_prompt=*/false, /*is_server_wallet=*/true),
        l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_ANDROID)
            IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_1_BRANDED
#else
            IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED
#endif
            ));

    EXPECT_EQ(
        GetPromptTitle(EntityTypeName::kDriversLicense, /*is_save_prompt=*/true,
                       /*is_banner_prompt=*/false, /*is_server_wallet=*/true),
        l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_ANDROID)
            IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_1_BRANDED
#else
            IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED
#endif
            ));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kAutofillAiWalletPassBranding2026, {{"string_variant", "2"}});

    // Variant 2 (save and update prompts)
    EXPECT_EQ(
        GetPromptTitle(EntityTypeName::kPassport, /*is_save_prompt=*/true,
                       /*is_banner_prompt=*/false, /*is_server_wallet=*/true),
        l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_ANDROID)
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_2_SECURELY
#else
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY
#endif
            ));

    EXPECT_EQ(
        GetPromptTitle(EntityTypeName::kPassport, /*is_save_prompt=*/false,
                       /*is_banner_prompt=*/false, /*is_server_wallet=*/true),
        l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_ANDROID)
            IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID_VARIANT_2_SECURELY
#else
            IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY
#endif
            ));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kAutofillAiWalletPassBranding2026, {{"string_variant", "99"}});

    // Invalid variant falls back to default branded
    EXPECT_EQ(
        GetPromptTitle(EntityTypeName::kPassport, /*is_save_prompt=*/true,
                       /*is_banner_prompt=*/false, /*is_server_wallet=*/true),
        l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_ANDROID)
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID_BRANDED
#else
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_BRANDED
#endif
            ));
  }
}

// Tests that GetPromptTitle returns unbranded strings for vehicles even when
// kAutofillAiWalletPassBranding2026 is enabled across all variants.
TEST(AutofillAiImportStringUtilsTest,
     GetPromptTitleVehicleExcludedFromBranding) {
  for (const std::string& variant : {"0", "1", "2", "99"}) {
    base::test::ScopedFeatureList feature_list;
    if (variant == "0") {
      feature_list.InitAndEnableFeature(
          features::kAutofillAiWalletPassBranding2026);
    } else {
      feature_list.InitAndEnableFeatureWithParameters(
          features::kAutofillAiWalletPassBranding2026,
          {{"string_variant", variant}});
    }

    // Vehicle save prompt must return unbranded title.
    EXPECT_EQ(
        GetPromptTitle(EntityTypeName::kVehicle, /*is_save_prompt=*/true,
                       /*is_banner_prompt=*/false, /*is_server_wallet=*/true),
        l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_ANDROID)
            IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE_ANDROID
#else
            IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE
#endif
            ));

    // Vehicle update prompt must return unbranded title.
    EXPECT_EQ(
        GetPromptTitle(EntityTypeName::kVehicle, /*is_save_prompt=*/false,
                       /*is_banner_prompt=*/false, /*is_server_wallet=*/true),
        l10n_util::GetStringUTF16(
#if BUILDFLAG(IS_ANDROID)
            IDS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE_ANDROID
#else
            IDS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE
#endif
            ));
  }
}

// Tests that GetPrimaryButtonTextId returns appropriate string IDs for save and
// update prompts.
TEST(AutofillAiImportStringUtilsTest, GetPrimaryButtonTextId) {
  EXPECT_EQ(
      GetPrimaryButtonTextId(/*is_save_prompt=*/true),
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON);
  EXPECT_EQ(
      GetPrimaryButtonTextId(/*is_save_prompt=*/false),
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_UPDATE_DIALOG_UPDATE_BUTTON);
}

// Tests that `GetWalletNoticeStringId()` returns the Wallet notice string for
// update prompts.
TEST(AutofillAiImportStringUtilsTest, GetWalletNoticeStringId_UpdatePrompt) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillRegionalizedPrivatePassesUxRefine);
  EXPECT_EQ(GetWalletNoticeStringId(EntityType(EntityTypeName::kPassport),
                                    /*is_save_prompt=*/false),
            IDS_AUTOFILL_AI_UPDATE_ENTITY_TO_WALLET_DIALOG_SUBTITLE);
}

// Tests that `GetWalletNoticeStringId()` returns the correct Wallet notice
// string for save prompts of private passes if
// `kAutofillRegionalizedPrivatePassesUxRefine` is disabled.
TEST(AutofillAiImportStringUtilsTest,
     GetWalletNoticeStringId_UpdatedPrivatePassNoticeDisabled) {
  base::test::ScopedFeatureList feature_list(
      /*enabled_features=*/{features::kAutofillAiWalletPrivatePasses},
      /*disabled_features=*/{
          features::kAutofillRegionalizedPrivatePassesUxRefine});
  EXPECT_EQ(GetWalletNoticeStringId(EntityType(EntityTypeName::kPassport),
                                    /*is_save_prompt=*/true),
            IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_DIALOG_SUBTITLE_NEW);
}

// Tests that `GetWalletNoticeStringId()` returns the correct Wallet notice
// string for save prompts of public and private passes if
// `kAutofillRegionalizedPrivatePassesUxRefine` is enabled.
TEST(AutofillAiImportStringUtilsTest,
     GetWalletNoticeStringId_UpdatedPrivatePassNoticeEnabled) {
  base::test::ScopedFeatureList feature_list(
      /*enabled_features=*/{features::kAutofillAiWalletPrivatePasses,
                            features::
                                kAutofillRegionalizedPrivatePassesUxRefine},
      /*disabled_features=*/{});
  EXPECT_EQ(GetWalletNoticeStringId(EntityType(EntityTypeName::kPassport),
                                    /*is_save_prompt=*/true),
            IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_DIALOG_SUBTITLE_PRIVATE_PASS);
  EXPECT_EQ(GetWalletNoticeStringId(EntityType(EntityTypeName::kVehicle),
                                    /*is_save_prompt=*/true),
            IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_DIALOG_SUBTITLE_NEW);
}

#if BUILDFLAG(IS_ANDROID)

// Tests that `GetAndroidWalletSourceNotice()` returns the correct Wallet
// notices for private passes.
TEST(AutofillAiImportStringUtilsTest, GetAndroidWalletSourceNotice) {
  base::test::ScopedFeatureList feature_list(
      /*enabled_features=*/{features::kAutofillAiWalletPrivatePasses},
      /*disabled_features=*/{
          features::kAutofillRegionalizedPrivatePassesUxRefine});

  EXPECT_EQ(GetAndroidWalletSourceNotice(EntityType(EntityTypeName::kPassport),
                                         /*is_save_prompt=*/true,
                                         /*account_email=*/u""),
            AndroidWalletSourceNotice{});

  AndroidWalletSourceNotice save_notice =
      GetAndroidWalletSourceNotice(EntityType(EntityTypeName::kPassport),
                                   /*is_save_prompt=*/true, u"user@gmail.com");
  EXPECT_THAT(
      save_notice.string_ids,
      ElementsAre(IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_DIALOG_SUBTITLE_NEW,
                  IDS_AUTOFILL_GOOGLE_WALLET_TITLE,
                  IDS_AUTOFILL_MANAGE_YOUR_INFO_LINK));
  EXPECT_THAT(base::UTF16ToUTF8(save_notice.text),
              AllOf(HasSubstr("<link>manage your info</link>"),
                    Not(HasSubstr("<link>Google Wallet</link>"))));

  AndroidWalletSourceNotice update_notice =
      GetAndroidWalletSourceNotice(EntityType(EntityTypeName::kPassport),
                                   /*is_save_prompt=*/false, u"user@gmail.com");
  EXPECT_THAT(
      update_notice.string_ids,
      ElementsAre(IDS_AUTOFILL_AI_UPDATE_ENTITY_TO_WALLET_DIALOG_SUBTITLE,
                  IDS_AUTOFILL_GOOGLE_WALLET_TITLE));
  EXPECT_THAT(base::UTF16ToUTF8(update_notice.text),
              HasSubstr("<link>Google Wallet</link>"));
}

// Tests that `GetAndroidWalletSourceNotice()` returns the correct private pass
// Wallet save notice when `kAutofillRegionalizedPrivatePassesUxRefine` is
// enabled.
TEST(AutofillAiImportStringUtilsTest,
     GetAndroidWalletSourceNotice_UpdatedPrivatePassNoticeEnabled) {
  base::test::ScopedFeatureList feature_list(
      /*enabled_features=*/{features::kAutofillAiWalletPrivatePasses,
                            features::
                                kAutofillRegionalizedPrivatePassesUxRefine},
      /*disabled_features=*/{});
  AndroidWalletSourceNotice private_pass_notice =
      GetAndroidWalletSourceNotice(EntityType(EntityTypeName::kPassport),
                                   /*is_save_prompt=*/true, u"user@gmail.com");
  EXPECT_THAT(
      private_pass_notice.string_ids,
      ElementsAre(
          IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_DIALOG_SUBTITLE_PRIVATE_PASS,
          IDS_AUTOFILL_GOOGLE_WALLET_TITLE,
          IDS_AUTOFILL_HOW_THIS_INFO_IS_USED_ACROSS_GOOGLE_LINK,
          IDS_AUTOFILL_PRIVACY_POLICY_LINK));
  EXPECT_THAT(
      base::UTF16ToUTF8(private_pass_notice.text),
      AllOf(HasSubstr("<link>how this info is used across Google</link>"),
            HasSubstr("<privacy_link>Privacy Policy</privacy_link>")));
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace autofill
