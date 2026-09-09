// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/account_preview_utils.h"

#include <algorithm>
#include <string>
#include <tuple>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace signin {

namespace {

int GetExpectedPromoMessageId(syncer::DataType data_type, bool with_device) {
  switch (data_type) {
    case syncer::PASSWORDS:
      return with_device
                 ? IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_PASSWORDS_WITH_DEVICE
                 : IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_PASSWORDS;
    case syncer::BOOKMARKS:
      return with_device
                 ? IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_BOOKMARKS_WITH_DEVICE
                 : IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_BOOKMARKS;
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_WALLET_METADATA:
      return with_device
                 ? IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_SAVED_INFO_WITH_DEVICE
                 : IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_SAVED_INFO;
    case syncer::READING_LIST:
      return with_device
                 ? IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_READING_LIST_WITH_DEVICE
                 : IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_READING_LIST;
    default:
      NOTREACHED();
  }
}

int GetExpectedProfileSeparationMessageId(syncer::DataType data_type,
                                          bool with_device) {
  switch (data_type) {
    case syncer::PASSWORDS:
      return with_device
                 ? IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_PASSWORDS_WITH_DEVICE
                 : IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_PASSWORDS;
    case syncer::BOOKMARKS:
      return with_device
                 ? IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_BOOKMARKS_WITH_DEVICE
                 : IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_BOOKMARKS;
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_WALLET_METADATA:
      return with_device
                 ? IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_SAVED_INFO_WITH_DEVICE
                 : IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_SAVED_INFO;
    case syncer::READING_LIST:
      return with_device
                 ? IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_READING_LIST_WITH_DEVICE
                 : IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_READING_LIST;
    default:
      NOTREACHED();
  }
}

std::string ParamToTestName(
    const testing::TestParamInfo<std::tuple<syncer::DataType, bool>>& info) {
  auto [data_type, with_device] = info.param;
  std::string name =
      base::StrCat({syncer::DataTypeToDebugString(data_type), "_",
                    with_device ? "WithDevice" : "WithoutDevice"});
  std::ranges::replace(name, ' ', '_');
  return name;
}

}  // namespace

TEST(AccountPreviewUtilsTest, EmptyPreferenceReturnsNullopt) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), std::nullopt);
}

class AccountPreviewUtilsPromoParamTest
    : public testing::TestWithParam<
          std::tuple<syncer::DataType /*data_type*/, bool /*with_device*/>> {};

TEST_P(AccountPreviewUtilsPromoParamTest, Subtitle) {
  const auto& [data_type, with_device] = GetParam();
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back({data_type, SyncDataQuartile::kAboveQ3});
  if (with_device) {
    pref.other_device_form_factor =
        sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE;
    std::u16string device_str =
        l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_PHONE);
    EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref),
              l10n_util::GetStringFUTF8(
                  GetExpectedPromoMessageId(data_type, /*with_device=*/true),
                  device_str));
  } else {
    EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref),
              l10n_util::GetStringUTF8(
                  GetExpectedPromoMessageId(data_type, /*with_device=*/false)));
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AccountPreviewUtilsPromoParamTest,
    testing::Combine(testing::Values(syncer::PASSWORDS,
                                     syncer::BOOKMARKS,
                                     syncer::AUTOFILL,
                                     syncer::AUTOFILL_WALLET_METADATA,
                                     syncer::READING_LIST),
                     /*with_device=*/testing::Bool()),
    ParamToTestName);

TEST(AccountPreviewUtilsTest, PreferenceWithExtensionsAndDesktopDevice) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::EXTENSIONS, SyncDataQuartile::kAboveQ3});
  pref.other_device_form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::u16string device_str =
      l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_COMPUTER);
  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_EXTENSIONS_WITH_DEVICE, device_str);

  EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), expected);
#else
  EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), std::nullopt);
#endif
}

TEST(AccountPreviewUtilsTest, PreferenceWithExtensionsAndNonDesktopDevice) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::EXTENSIONS, SyncDataQuartile::kAboveQ3});
  pref.preferred_data_types.push_back(
      {syncer::PASSWORDS, SyncDataQuartile::kAboveQ3});
  pref.other_device_form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE;

  std::u16string device_str =
      l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_PHONE);
  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_PASSWORDS_WITH_DEVICE, device_str);

  EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), expected);
}

TEST(AccountPreviewUtilsTest, PreferenceWithExtensionsWithoutDevice) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::EXTENSIONS, SyncDataQuartile::kAboveQ3});

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::string expected =
      l10n_util::GetStringUTF8(IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_EXTENSIONS);
  EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), expected);
#else
  EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), std::nullopt);
#endif
}

TEST(AccountPreviewUtilsTest,
     PreferenceWithExtensionsWithoutDeviceFollowedBySupportedType) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::EXTENSIONS, SyncDataQuartile::kAboveQ3});
  pref.preferred_data_types.push_back(
      {syncer::PASSWORDS, SyncDataQuartile::kAboveQ3});

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::string expected =
      l10n_util::GetStringUTF8(IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_EXTENSIONS);
#else
  std::string expected =
      l10n_util::GetStringUTF8(IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_PASSWORDS);
#endif

  EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), expected);
}

TEST(AccountPreviewUtilsTest,
     PreferenceWithExtensionsAndDesktopDeviceFollowedBySupportedType) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::EXTENSIONS, SyncDataQuartile::kAboveQ3});
  pref.preferred_data_types.push_back(
      {syncer::PASSWORDS, SyncDataQuartile::kAboveQ3});
  pref.other_device_form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP;

  std::u16string device_str =
      l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_COMPUTER);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_EXTENSIONS_WITH_DEVICE, device_str);
#else
  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_PASSWORDS_WITH_DEVICE, device_str);
#endif

  EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), expected);
}

TEST(AccountPreviewUtilsTest, UnsupportedDataTypeReturnsNullopt) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::HISTORY, SyncDataQuartile::kAboveQ3});

  EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), std::nullopt);
}

TEST(AccountPreviewUtilsTest,
     UnsupportedDataTypeFollowedBySupportedDataTypePicksFirstSupported) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::HISTORY, SyncDataQuartile::kAboveQ3});
  pref.preferred_data_types.push_back(
      {syncer::PASSWORDS, SyncDataQuartile::kAboveQ3});

  std::string expected =
      l10n_util::GetStringUTF8(IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_PASSWORDS);

  EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), expected);
}

TEST(AccountPreviewUtilsTest, ProfileSeparationEmptyPreferenceReturnsNullopt) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                "Elisa", "bob.beckett@gmail.com", pref),
            std::nullopt);
}

class AccountPreviewUtilsProfileSeparationParamTest
    : public testing::TestWithParam<
          std::tuple<syncer::DataType /*data_type*/, bool /*with_device*/>> {};

TEST_P(AccountPreviewUtilsProfileSeparationParamTest, Subtitle) {
  const auto& [data_type, with_device] = GetParam();
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back({data_type, SyncDataQuartile::kAboveQ3});
  if (with_device) {
    pref.other_device_form_factor =
        sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE;
    std::u16string device_str =
        l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_PHONE);
    EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                  "Elisa", "bob.beckett@gmail.com", pref),
              l10n_util::GetStringFUTF8(
                  GetExpectedProfileSeparationMessageId(data_type,
                                                        /*with_device=*/true),
                  u"Elisa", u"bob.beckett@gmail.com", device_str));
  } else {
    EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                  "Elisa", "bob.beckett@gmail.com", pref),
              l10n_util::GetStringFUTF8(
                  GetExpectedProfileSeparationMessageId(data_type,
                                                        /*with_device=*/false),
                  u"Elisa", u"bob.beckett@gmail.com"));
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AccountPreviewUtilsProfileSeparationParamTest,
    testing::Combine(testing::Values(syncer::PASSWORDS,
                                     syncer::BOOKMARKS,
                                     syncer::AUTOFILL,
                                     syncer::AUTOFILL_WALLET_METADATA,
                                     syncer::READING_LIST),
                     /*with_device=*/testing::Bool()),
    ParamToTestName);

TEST(AccountPreviewUtilsTest, ProfileSeparationWithExtensionsAndDesktopDevice) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::EXTENSIONS, SyncDataQuartile::kAboveQ3});
  pref.other_device_form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::u16string device_str =
      l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_COMPUTER);
  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_EXTENSIONS_WITH_DEVICE,
      u"Elisa", u"bob.beckett@gmail.com", device_str);

  EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                "Elisa", "bob.beckett@gmail.com", pref),
            expected);
#else
  EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                "Elisa", "bob.beckett@gmail.com", pref),
            std::nullopt);
#endif
}

TEST(AccountPreviewUtilsTest,
     ProfileSeparationWithExtensionsAndNonDesktopDevice) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::EXTENSIONS, SyncDataQuartile::kAboveQ3});
  pref.preferred_data_types.push_back(
      {syncer::PASSWORDS, SyncDataQuartile::kAboveQ3});
  pref.other_device_form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE;

  std::u16string device_str =
      l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_PHONE);
  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_PASSWORDS_WITH_DEVICE,
      u"Elisa", u"bob.beckett@gmail.com", device_str);

  EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                "Elisa", "bob.beckett@gmail.com", pref),
            expected);
}

TEST(AccountPreviewUtilsTest, ProfileSeparationWithExtensionsWithoutDevice) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::EXTENSIONS, SyncDataQuartile::kAboveQ3});

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_EXTENSIONS, u"Elisa",
      u"bob.beckett@gmail.com");
  EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                "Elisa", "bob.beckett@gmail.com", pref),
            expected);
#else
  EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                "Elisa", "bob.beckett@gmail.com", pref),
            std::nullopt);
#endif
}

TEST(AccountPreviewUtilsTest,
     ProfileSeparationWithExtensionsWithoutDeviceFollowedBySupportedType) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::EXTENSIONS, SyncDataQuartile::kAboveQ3});
  pref.preferred_data_types.push_back(
      {syncer::PASSWORDS, SyncDataQuartile::kAboveQ3});

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_EXTENSIONS, u"Elisa",
      u"bob.beckett@gmail.com");
#else
  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_PASSWORDS, u"Elisa",
      u"bob.beckett@gmail.com");
#endif

  EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                "Elisa", "bob.beckett@gmail.com", pref),
            expected);
}

TEST(
    AccountPreviewUtilsTest,
    ProfileSeparationPreferenceWithExtensionsAndDesktopDeviceFollowedBySupportedType) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::EXTENSIONS, SyncDataQuartile::kAboveQ3});
  pref.preferred_data_types.push_back(
      {syncer::PASSWORDS, SyncDataQuartile::kAboveQ3});
  pref.other_device_form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP;

  std::u16string device_str =
      l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_COMPUTER);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_EXTENSIONS_WITH_DEVICE,
      u"Elisa", u"bob.beckett@gmail.com", device_str);
#else
  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_PASSWORDS_WITH_DEVICE,
      u"Elisa", u"bob.beckett@gmail.com", device_str);
#endif

  EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                "Elisa", "bob.beckett@gmail.com", pref),
            expected);
}

TEST(AccountPreviewUtilsTest,
     ProfileSeparationUnsupportedDataTypeReturnsNullopt) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::HISTORY, SyncDataQuartile::kAboveQ3});

  EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                "Elisa", "bob.beckett@gmail.com", pref),
            std::nullopt);
}

TEST(
    AccountPreviewUtilsTest,
    ProfileSeparationUnsupportedDataTypeFollowedBySupportedDataTypePicksFirstSupported) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::HISTORY, SyncDataQuartile::kAboveQ3});
  pref.preferred_data_types.push_back(
      {syncer::PASSWORDS, SyncDataQuartile::kAboveQ3});

  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_PASSWORDS, u"Elisa",
      u"bob.beckett@gmail.com");

  EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                "Elisa", "bob.beckett@gmail.com", pref),
            expected);
}

}  // namespace signin
