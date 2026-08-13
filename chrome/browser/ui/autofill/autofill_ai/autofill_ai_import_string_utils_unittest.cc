// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_string_utils.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

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

// Tests that GetPrimaryButtonTextId returns appropriate string IDs for save and update prompts.
TEST(AutofillAiImportStringUtilsTest, GetPrimaryButtonTextId) {
  EXPECT_EQ(
      GetPrimaryButtonTextId(/*is_save_prompt=*/true),
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON);
  EXPECT_EQ(
      GetPrimaryButtonTextId(/*is_save_prompt=*/false),
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_UPDATE_DIALOG_UPDATE_BUTTON);
}

}  // namespace
}  // namespace autofill
