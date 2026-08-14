// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/account_preview_utils.h"

#include <string>
#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "ui/base/l10n/l10n_util.h"

namespace signin {

namespace {

std::optional<int> GetDeviceStringId(
    sync_pb::SyncEnums_DeviceFormFactor form_factor) {
  switch (form_factor) {
    case sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE:
      return IDS_ACCOUNT_PREVIEW_DEVICE_PHONE;
    case sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_TABLET:
      return IDS_ACCOUNT_PREVIEW_DEVICE_TABLET;
    case sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP:
      return IDS_ACCOUNT_PREVIEW_DEVICE_COMPUTER;
    default:
      return std::nullopt;
  }
}

std::optional<int> GetPromoMessageId(syncer::DataType data_type,
                                     bool has_device) {
  switch (data_type) {
    case syncer::PASSWORDS:
      return has_device
                 ? IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_PASSWORDS_WITH_DEVICE
                 : IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_PASSWORDS;
    case syncer::BOOKMARKS:
      return has_device
                 ? IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_BOOKMARKS_WITH_DEVICE
                 : IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_BOOKMARKS;
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_WALLET_METADATA:
      return has_device
                 ? IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_SAVED_INFO_WITH_DEVICE
                 : IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_SAVED_INFO;
    default:
      return std::nullopt;
  }
}

std::optional<int> GetProfileSeparationMessageId(syncer::DataType data_type,
                                                 bool has_device) {
  switch (data_type) {
    case syncer::PASSWORDS:
      return has_device
                 ? IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_PASSWORDS_WITH_DEVICE
                 : IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_PASSWORDS;
    case syncer::BOOKMARKS:
      return has_device
                 ? IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_BOOKMARKS_WITH_DEVICE
                 : IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_BOOKMARKS;
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_WALLET_METADATA:
      return has_device
                 ? IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_SAVED_INFO_WITH_DEVICE
                 : IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_SAVED_INFO;
    default:
      return std::nullopt;
  }
}

}  // namespace

std::optional<std::string> GetAccountPreviewPromoSubtitle(
    const AccountPreviewDataService::AccountPreviewPreference& preference) {
  std::optional<int> device_string_id =
      GetDeviceStringId(preference.other_device_form_factor);
  bool has_device = device_string_id.has_value();

  for (const auto& preferred_data_type : preference.preferred_data_types) {
    std::optional<int> message_id =
        GetPromoMessageId(preferred_data_type.data_type, has_device);
    if (!message_id) {
      continue;
    }

    if (has_device) {
      return l10n_util::GetStringFUTF8(
          *message_id, l10n_util::GetStringUTF16(*device_string_id));
    }

    return l10n_util::GetStringUTF8(*message_id);
  }

  return std::nullopt;
}

std::optional<std::string> GetAccountPreviewProfileSeparationSubtitle(
    std::string_view existing_account_given_name,
    std::string_view new_account_email,
    const AccountPreviewDataService::AccountPreviewPreference& preference) {
  std::optional<int> device_string_id =
      GetDeviceStringId(preference.other_device_form_factor);
  bool has_device = device_string_id.has_value();

  for (const auto& preferred_data_type : preference.preferred_data_types) {
    std::optional<int> message_id = GetProfileSeparationMessageId(
        preferred_data_type.data_type, has_device);
    if (!message_id) {
      continue;
    }

    std::u16string existing_user =
        base::UTF8ToUTF16(existing_account_given_name);
    std::u16string new_user_email_u16 = base::UTF8ToUTF16(new_account_email);

    if (has_device) {
      return l10n_util::GetStringFUTF8(
          *message_id, existing_user, new_user_email_u16,
          l10n_util::GetStringUTF16(*device_string_id));
    }

    return l10n_util::GetStringFUTF8(*message_id, existing_user,
                                     new_user_email_u16);
  }

  return std::nullopt;
}

}  // namespace signin
