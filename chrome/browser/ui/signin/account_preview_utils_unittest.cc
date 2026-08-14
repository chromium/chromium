// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/account_preview_utils.h"

#include <string>

#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace signin {

TEST(AccountPreviewUtilsTest, EmptyPreferenceReturnsNullopt) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), std::nullopt);
}

TEST(AccountPreviewUtilsTest, PreferenceWithPasswordsAndDevice) {
  AccountPreviewDataService::AccountPreviewPreference pref;
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

TEST(AccountPreviewUtilsTest, PreferenceWithBookmarksAndDevice) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::BOOKMARKS, SyncDataQuartile::kAboveQ3});
  pref.other_device_form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_TABLET;

  std::u16string device_str =
      l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_TABLET);
  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_BOOKMARKS_WITH_DEVICE, device_str);

  EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), expected);
}

TEST(AccountPreviewUtilsTest, PreferenceWithSavedInfoAndDevice) {
  {
    AccountPreviewDataService::AccountPreviewPreference pref;
    pref.preferred_data_types.push_back(
        {syncer::AUTOFILL, SyncDataQuartile::kAboveQ3});
    pref.other_device_form_factor =
        sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP;

    std::u16string device_str =
        l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_COMPUTER);
    std::string expected = l10n_util::GetStringFUTF8(
        IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_SAVED_INFO_WITH_DEVICE, device_str);

    EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), expected);
  }

  {
    AccountPreviewDataService::AccountPreviewPreference pref;
    pref.preferred_data_types.push_back(
        {syncer::AUTOFILL_WALLET_METADATA, SyncDataQuartile::kAboveQ3});
    pref.other_device_form_factor =
        sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE;

    std::u16string device_str =
        l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_PHONE);
    std::string expected = l10n_util::GetStringFUTF8(
        IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_SAVED_INFO_WITH_DEVICE, device_str);

    EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), expected);
  }
}

TEST(AccountPreviewUtilsTest, PreferenceWithPasswordsWithoutDevice) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::PASSWORDS, SyncDataQuartile::kAboveQ3});

  std::string expected =
      l10n_util::GetStringUTF8(IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_PASSWORDS);

  EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), expected);
}

TEST(AccountPreviewUtilsTest, PreferenceWithBookmarksWithoutDevice) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::BOOKMARKS, SyncDataQuartile::kAboveQ3});

  std::string expected =
      l10n_util::GetStringUTF8(IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_BOOKMARKS);

  EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), expected);
}

TEST(AccountPreviewUtilsTest, PreferenceWithSavedInfoWithoutDevice) {
  {
    AccountPreviewDataService::AccountPreviewPreference pref;
    pref.preferred_data_types.push_back(
        {syncer::AUTOFILL, SyncDataQuartile::kAboveQ3});

    std::string expected =
        l10n_util::GetStringUTF8(IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_SAVED_INFO);

    EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), expected);
  }

  {
    AccountPreviewDataService::AccountPreviewPreference pref;
    pref.preferred_data_types.push_back(
        {syncer::AUTOFILL_WALLET_METADATA, SyncDataQuartile::kAboveQ3});

    std::string expected =
        l10n_util::GetStringUTF8(IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_SAVED_INFO);

    EXPECT_EQ(GetAccountPreviewPromoSubtitle(pref), expected);
  }
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

TEST(AccountPreviewUtilsTest, ProfileSeparationWithPasswordsAndDevice) {
  AccountPreviewDataService::AccountPreviewPreference pref;
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

TEST(AccountPreviewUtilsTest, ProfileSeparationWithBookmarksAndDevice) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::BOOKMARKS, SyncDataQuartile::kAboveQ3});
  pref.other_device_form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_TABLET;

  std::u16string device_str =
      l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_TABLET);
  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_BOOKMARKS_WITH_DEVICE,
      u"Elisa", u"bob.beckett@gmail.com", device_str);

  EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                "Elisa", "bob.beckett@gmail.com", pref),
            expected);
}

TEST(AccountPreviewUtilsTest, ProfileSeparationWithSavedInfoAndDevice) {
  {
    AccountPreviewDataService::AccountPreviewPreference pref;
    pref.preferred_data_types.push_back(
        {syncer::AUTOFILL, SyncDataQuartile::kAboveQ3});
    pref.other_device_form_factor =
        sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP;

    std::u16string device_str =
        l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_COMPUTER);
    std::string expected = l10n_util::GetStringFUTF8(
        IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_SAVED_INFO_WITH_DEVICE,
        u"Elisa", u"bob.beckett@gmail.com", device_str);

    EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                  "Elisa", "bob.beckett@gmail.com", pref),
              expected);
  }

  {
    AccountPreviewDataService::AccountPreviewPreference pref;
    pref.preferred_data_types.push_back(
        {syncer::AUTOFILL_WALLET_METADATA, SyncDataQuartile::kAboveQ3});
    pref.other_device_form_factor =
        sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE;

    std::u16string device_str =
        l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_PHONE);
    std::string expected = l10n_util::GetStringFUTF8(
        IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_SAVED_INFO_WITH_DEVICE,
        u"Elisa", u"bob.beckett@gmail.com", device_str);

    EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                  "Elisa", "bob.beckett@gmail.com", pref),
              expected);
  }
}

TEST(AccountPreviewUtilsTest, ProfileSeparationWithPasswordsWithoutDevice) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::PASSWORDS, SyncDataQuartile::kAboveQ3});

  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_PASSWORDS, u"Elisa",
      u"bob.beckett@gmail.com");

  EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                "Elisa", "bob.beckett@gmail.com", pref),
            expected);
}

TEST(AccountPreviewUtilsTest, ProfileSeparationWithBookmarksWithoutDevice) {
  AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::BOOKMARKS, SyncDataQuartile::kAboveQ3});

  std::string expected = l10n_util::GetStringFUTF8(
      IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_BOOKMARKS, u"Elisa",
      u"bob.beckett@gmail.com");

  EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                "Elisa", "bob.beckett@gmail.com", pref),
            expected);
}

TEST(AccountPreviewUtilsTest, ProfileSeparationWithSavedInfoWithoutDevice) {
  {
    AccountPreviewDataService::AccountPreviewPreference pref;
    pref.preferred_data_types.push_back(
        {syncer::AUTOFILL, SyncDataQuartile::kAboveQ3});

    std::string expected = l10n_util::GetStringFUTF8(
        IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_SAVED_INFO, u"Elisa",
        u"bob.beckett@gmail.com");

    EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                  "Elisa", "bob.beckett@gmail.com", pref),
              expected);
  }

  {
    AccountPreviewDataService::AccountPreviewPreference pref;
    pref.preferred_data_types.push_back(
        {syncer::AUTOFILL_WALLET_METADATA, SyncDataQuartile::kAboveQ3});

    std::string expected = l10n_util::GetStringFUTF8(
        IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_SAVED_INFO, u"Elisa",
        u"bob.beckett@gmail.com");

    EXPECT_EQ(GetAccountPreviewProfileSeparationSubtitle(
                  "Elisa", "bob.beckett@gmail.com", pref),
              expected);
  }
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
