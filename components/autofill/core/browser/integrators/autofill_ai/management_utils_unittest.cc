// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/management_utils.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// Tests that `ResolveStringIdsForWalletPass2026Experiment()` resolves string
// IDs among the branded experiment arms based on feature parameter `string_variant`.
TEST(ManagementUtilsTest, ResolveStringIdsForWalletPass2026Experiment) {
  constexpr int kDefaultBrandedId = 100;
  constexpr int kVariant1Id = 101;
  constexpr int kVariant2Id = 102;

  // Default branded ID returned when feature parameter is 0 or unset (Arm 2).
  EXPECT_EQ(ResolveStringIdsForWalletPass2026Experiment(
                kDefaultBrandedId, kVariant1Id, kVariant2Id),
            kDefaultBrandedId);

  // Variant 1 ID returned when feature parameter string_variant is 1 (Arm 3).
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kAutofillAiWalletPassBranding2026, {{"string_variant", "1"}});
    EXPECT_EQ(ResolveStringIdsForWalletPass2026Experiment(
                  kDefaultBrandedId, kVariant1Id, kVariant2Id),
              kVariant1Id);
  }

  // Variant 2 ID returned when feature parameter string_variant is 2 (Arm 4).
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kAutofillAiWalletPassBranding2026, {{"string_variant", "2"}});
    EXPECT_EQ(ResolveStringIdsForWalletPass2026Experiment(
                  kDefaultBrandedId, kVariant1Id, kVariant2Id),
              kVariant2Id);
  }

  // Fallback to default branded ID for unknown variant numbers.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kAutofillAiWalletPassBranding2026, {{"string_variant", "99"}});
    EXPECT_EQ(ResolveStringIdsForWalletPass2026Experiment(
                  kDefaultBrandedId, kVariant1Id, kVariant2Id),
              kDefaultBrandedId);
  }
}

// Tests that `GetEntityInstancesForSettings()` drops pContext entities.
TEST(ManagementUtilsTest, GetEntityInstancesForSettings) {
  EntityInstance local_passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kLocal});
  EntityInstance wallet_vehicle = test::GetVehicleEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  EntityInstance pcontext_order = test::GetOrderEntityInstance(
      {.record_type = EntityInstance::RecordType::kPersonalContext});
  EXPECT_THAT(GetEntityInstancesForSettings(
                  {local_passport, pcontext_order, wallet_vehicle}),
              testing::ElementsAre(local_passport, wallet_vehicle));
}

// Tests that `GetAddEntityTypeStringForI18n()` returns the correct localized
// string for writable entity types (with branding options when applicable) and
// empty strings for read-only entity types.
TEST(ManagementUtilsTest, GetAddEntityTypeStringForI18n) {
  EXPECT_EQ(GetAddEntityTypeStringForI18n(EntityType(EntityTypeName::kPassport),
                                          /*is_wallet_branded=*/false),
            l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_ADD_PASSPORT_ENTITY));
  EXPECT_EQ(
      GetAddEntityTypeStringForI18n(EntityType(EntityTypeName::kDriversLicense),
                                    /*is_wallet_branded=*/false),
      l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_ADD_DRIVERS_LICENSE_ENTITY));

  EXPECT_EQ(GetAddEntityTypeStringForI18n(
                EntityType(EntityTypeName::kFlightReservation),
                /*is_wallet_branded=*/false),
            "");
  EXPECT_EQ(GetAddEntityTypeStringForI18n(
                EntityType(EntityTypeName::kFlightReservation),
                /*is_wallet_branded=*/true),
            "");

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
  // Verify baseline (default variant 0) branded strings.
  EXPECT_EQ(GetAddEntityTypeStringForI18n(EntityType(EntityTypeName::kPassport),
                                          /*is_wallet_branded=*/true),
            l10n_util::GetStringUTF8(
                IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_BRANDED));

  // Verify string serving for variant 1 across entity types.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kAutofillAiWalletPassBranding2026, {{"string_variant", "1"}});
    EXPECT_EQ(
        GetAddEntityTypeStringForI18n(EntityType(EntityTypeName::kPassport),
                                      /*is_wallet_branded=*/true),
        l10n_util::GetStringUTF8(
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED));
    EXPECT_EQ(
        GetAddEntityTypeStringForI18n(
            EntityType(EntityTypeName::kDriversLicense),
            /*is_wallet_branded=*/true),
        l10n_util::GetStringUTF8(
            IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED));
    EXPECT_EQ(
        GetAddEntityTypeStringForI18n(
            EntityType(EntityTypeName::kVehicle),
            /*is_wallet_branded=*/true),
        l10n_util::GetStringUTF8(
            IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED));
    EXPECT_EQ(
        GetAddEntityTypeStringForI18n(
            EntityType(EntityTypeName::kNationalIdCard),
            /*is_wallet_branded=*/true),
        l10n_util::GetStringUTF8(
            IDS_AUTOFILL_AI_SAVE_ID_CARD_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED));
    EXPECT_EQ(
        GetAddEntityTypeStringForI18n(
            EntityType(EntityTypeName::kKnownTravelerNumber),
            /*is_wallet_branded=*/true),
        l10n_util::GetStringUTF8(
            IDS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED));
    EXPECT_EQ(
        GetAddEntityTypeStringForI18n(
            EntityType(EntityTypeName::kRedressNumber),
            /*is_wallet_branded=*/true),
        l10n_util::GetStringUTF8(
            IDS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_VARIANT_1_BRANDED));
  }

  // Verify string serving for variant 2 across entity types.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kAutofillAiWalletPassBranding2026, {{"string_variant", "2"}});
    EXPECT_EQ(
        GetAddEntityTypeStringForI18n(EntityType(EntityTypeName::kPassport),
                                      /*is_wallet_branded=*/true),
        l10n_util::GetStringUTF8(
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY));
    EXPECT_EQ(
        GetAddEntityTypeStringForI18n(
            EntityType(EntityTypeName::kDriversLicense),
            /*is_wallet_branded=*/true),
        l10n_util::GetStringUTF8(
            IDS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY));
    EXPECT_EQ(
        GetAddEntityTypeStringForI18n(
            EntityType(EntityTypeName::kVehicle),
            /*is_wallet_branded=*/true),
        l10n_util::GetStringUTF8(
            IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY));
    EXPECT_EQ(
        GetAddEntityTypeStringForI18n(
            EntityType(EntityTypeName::kNationalIdCard),
            /*is_wallet_branded=*/true),
        l10n_util::GetStringUTF8(
            IDS_AUTOFILL_AI_SAVE_ID_CARD_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY));
    EXPECT_EQ(
        GetAddEntityTypeStringForI18n(
            EntityType(EntityTypeName::kKnownTravelerNumber),
            /*is_wallet_branded=*/true),
        l10n_util::GetStringUTF8(
            IDS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY));
    EXPECT_EQ(
        GetAddEntityTypeStringForI18n(
            EntityType(EntityTypeName::kRedressNumber),
            /*is_wallet_branded=*/true),
        l10n_util::GetStringUTF8(
            IDS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE_VARIANT_2_SECURELY));
  }

  // Verify fallback to default branded strings for invalid variant parameters.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kAutofillAiWalletPassBranding2026, {{"string_variant", "99"}});
    EXPECT_EQ(
        GetAddEntityTypeStringForI18n(EntityType(EntityTypeName::kPassport),
                                      /*is_wallet_branded=*/true),
        l10n_util::GetStringUTF8(
            IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_BRANDED));
  }
#else
  EXPECT_EQ(GetAddEntityTypeStringForI18n(EntityType(EntityTypeName::kPassport),
                                          /*is_wallet_branded=*/true),
            l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_ADD_PASSPORT_ENTITY));
#endif
}

// Tests that `GetEntityTypeSectionTitleStringForI18n()` returns the correct
// localized section title for all entity types.
TEST(ManagementUtilsTest, GetEntityTypeSectionTitleStringForI18n) {
  EXPECT_EQ(GetEntityTypeSectionTitleStringForI18n(
                EntityType(EntityTypeName::kPassport)),
            l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_PASSPORTS_TITLE));
  EXPECT_EQ(
      GetEntityTypeSectionTitleStringForI18n(
          EntityType(EntityTypeName::kFlightReservation)),
      l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_FLIGHT_RESERVATIONS_TITLE));
}

// Tests that `GetEditEntityTypeStringForI18n()` returns the correct localized
// strings for writable types and empty strings for read-only types.
TEST(ManagementUtilsTest, GetEditEntityTypeStringForI18n) {
  EXPECT_EQ(
      GetEditEntityTypeStringForI18n(EntityType(EntityTypeName::kPassport)),
      l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_EDIT_PASSPORT_ENTITY));
  EXPECT_EQ(GetEditEntityTypeStringForI18n(
                EntityType(EntityTypeName::kFlightReservation)),
            "");
}

// Tests that `GetDeleteEntityTypeStringForI18n()` returns the correct localized
// strings for writable types and empty strings for read-only types.
TEST(ManagementUtilsTest, GetDeleteEntityTypeStringForI18n) {
  EXPECT_EQ(
      GetDeleteEntityTypeStringForI18n(EntityType(EntityTypeName::kPassport)),
      l10n_util::GetStringUTF8(IDS_AUTOFILL_AI_DELETE_PASSPORT_ENTITY));
  EXPECT_EQ(GetDeleteEntityTypeStringForI18n(
                EntityType(EntityTypeName::kFlightReservation)),
            "");
}

}  // namespace
}  // namespace autofill
