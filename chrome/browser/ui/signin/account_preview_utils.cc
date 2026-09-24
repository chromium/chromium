// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/account_preview_utils.h"

#include <string>
#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_buildflags.h"
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

std::optional<std::string> GetPromoSubtitleForDataType(
    syncer::DataType data_type,
    sync_pb::SyncEnums_DeviceFormFactor form_factor) {
  std::optional<int> device_string_id = GetDeviceStringId(form_factor);

  auto format_promo = [&](int id_with_device, int id_without_device) {
    return device_string_id.has_value()
               ? l10n_util::GetStringFUTF8(
                     id_with_device,
                     l10n_util::GetStringUTF16(*device_string_id))
               : l10n_util::GetStringUTF8(id_without_device);
  };

  switch (data_type) {
    case syncer::PASSWORDS:
      return format_promo(
          IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_PASSWORDS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_PASSWORDS);
    case syncer::BOOKMARKS:
      return format_promo(
          IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_BOOKMARKS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_BOOKMARKS);
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_WALLET_METADATA:
      return format_promo(
          IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_SAVED_INFO_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_SAVED_INFO);
    case syncer::READING_LIST:
      return format_promo(
          IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_READING_LIST_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_READING_LIST);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    case syncer::EXTENSIONS:
      // Only allow using extensions data type with generic devices (no specific
      // device) or with Desktop.
      if (device_string_id.has_value() &&
          form_factor !=
              sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP) {
        return std::nullopt;
      }
      return format_promo(
          IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_EXTENSIONS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_PROMO_SUBTITLE_EXTENSIONS);
#endif
    default:
      return std::nullopt;
  }
}

std::optional<std::string> GetProfileMenuSubtitleForDataType(
    syncer::DataType data_type,
    sync_pb::SyncEnums_DeviceFormFactor form_factor,
    const std::u16string& email) {
  std::optional<int> device_string_id = GetDeviceStringId(form_factor);

  auto format_profile_menu = [&](int id_with_device, int id_without_device) {
    return device_string_id.has_value()
               ? l10n_util::GetStringFUTF8(
                     id_with_device,
                     l10n_util::GetStringUTF16(*device_string_id), email)
               : l10n_util::GetStringFUTF8(id_without_device, email);
  };

  switch (data_type) {
    case syncer::PASSWORDS:
      return format_profile_menu(
          IDS_ACCOUNT_PREVIEW_PROFILE_MENU_SUBTITLE_PASSWORDS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_PROFILE_MENU_SUBTITLE_PASSWORDS);
    case syncer::BOOKMARKS:
      return format_profile_menu(
          IDS_ACCOUNT_PREVIEW_PROFILE_MENU_SUBTITLE_BOOKMARKS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_PROFILE_MENU_SUBTITLE_BOOKMARKS);
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_WALLET_METADATA:
      return format_profile_menu(
          IDS_ACCOUNT_PREVIEW_PROFILE_MENU_SUBTITLE_SAVED_INFO_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_PROFILE_MENU_SUBTITLE_SAVED_INFO);
    case syncer::READING_LIST:
      return format_profile_menu(
          IDS_ACCOUNT_PREVIEW_PROFILE_MENU_SUBTITLE_READING_LIST_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_PROFILE_MENU_SUBTITLE_READING_LIST);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    case syncer::EXTENSIONS:
      // Only allow using extensions data type with generic devices (no specific
      // device) or with Desktop.
      if (device_string_id.has_value() &&
          form_factor !=
              sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP) {
        return std::nullopt;
      }
      return format_profile_menu(
          IDS_ACCOUNT_PREVIEW_PROFILE_MENU_SUBTITLE_EXTENSIONS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_PROFILE_MENU_SUBTITLE_EXTENSIONS);
#endif
    default:
      return std::nullopt;
  }
}

std::optional<std::string> GetSettingsPromoSubtitleForDataType(
    syncer::DataType data_type,
    sync_pb::SyncEnums_DeviceFormFactor form_factor) {
  std::optional<int> device_string_id = GetDeviceStringId(form_factor);

  auto format_promo = [&](int id_with_device, int id_without_device) {
    return device_string_id.has_value()
               ? l10n_util::GetStringFUTF8(
                     id_with_device,
                     l10n_util::GetStringUTF16(*device_string_id))
               : l10n_util::GetStringUTF8(id_without_device);
  };

  switch (data_type) {
    case syncer::PASSWORDS:
      return format_promo(
          IDS_ACCOUNT_PREVIEW_SETTINGS_PROMO_SUBTITLE_PASSWORDS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_SETTINGS_PROMO_SUBTITLE_PASSWORDS);
    case syncer::BOOKMARKS:
      return format_promo(
          IDS_ACCOUNT_PREVIEW_SETTINGS_PROMO_SUBTITLE_BOOKMARKS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_SETTINGS_PROMO_SUBTITLE_BOOKMARKS);
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_WALLET_METADATA:
      return format_promo(
          IDS_ACCOUNT_PREVIEW_SETTINGS_PROMO_SUBTITLE_SAVED_INFO_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_SETTINGS_PROMO_SUBTITLE_SAVED_INFO);
    case syncer::READING_LIST:
      return format_promo(
          IDS_ACCOUNT_PREVIEW_SETTINGS_PROMO_SUBTITLE_READING_LIST_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_SETTINGS_PROMO_SUBTITLE_READING_LIST);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    case syncer::EXTENSIONS:
      // Only allow using extensions data type with generic devices (no specific
      // device) or with Desktop.
      if (device_string_id.has_value() &&
          form_factor !=
              sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP) {
        return std::nullopt;
      }
      return format_promo(
          IDS_ACCOUNT_PREVIEW_SETTINGS_PROMO_SUBTITLE_EXTENSIONS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_SETTINGS_PROMO_SUBTITLE_EXTENSIONS);
#endif
    default:
      return std::nullopt;
  }
}

std::optional<std::string> GetHistoryPromoSubtitleForDataType(
    syncer::DataType data_type,
    sync_pb::SyncEnums_DeviceFormFactor form_factor,
    const std::u16string& email) {
  std::optional<int> device_string_id = GetDeviceStringId(form_factor);

  auto format_history_promo = [&](int id_with_device, int id_without_device) {
    return device_string_id.has_value()
               ? l10n_util::GetStringFUTF8(
                     id_with_device,
                     l10n_util::GetStringUTF16(*device_string_id), email)
               : l10n_util::GetStringFUTF8(id_without_device, email);
  };

  switch (data_type) {
    case syncer::PASSWORDS:
      return format_history_promo(
          IDS_ACCOUNT_PREVIEW_HISTORY_PROMO_SUBTITLE_PASSWORDS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_HISTORY_PROMO_SUBTITLE_PASSWORDS);
    case syncer::BOOKMARKS:
      return format_history_promo(
          IDS_ACCOUNT_PREVIEW_HISTORY_PROMO_SUBTITLE_BOOKMARKS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_HISTORY_PROMO_SUBTITLE_BOOKMARKS);
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_WALLET_METADATA:
      return format_history_promo(
          IDS_ACCOUNT_PREVIEW_HISTORY_PROMO_SUBTITLE_SAVED_INFO_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_HISTORY_PROMO_SUBTITLE_SAVED_INFO);
    case syncer::READING_LIST:
      return format_history_promo(
          IDS_ACCOUNT_PREVIEW_HISTORY_PROMO_SUBTITLE_READING_LIST_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_HISTORY_PROMO_SUBTITLE_READING_LIST);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    case syncer::EXTENSIONS:
      // Only allow using extensions data type with generic devices (no specific
      // device) or with Desktop.
      if (device_string_id.has_value() &&
          form_factor !=
              sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP) {
        return std::nullopt;
      }
      return format_history_promo(
          IDS_ACCOUNT_PREVIEW_HISTORY_PROMO_SUBTITLE_EXTENSIONS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_HISTORY_PROMO_SUBTITLE_EXTENSIONS);
#endif
    default:
      return std::nullopt;
  }
}

std::optional<std::string> GetRecentTabsPromoSubtitleForDataType(
    syncer::DataType data_type,
    sync_pb::SyncEnums_DeviceFormFactor form_factor) {
  std::optional<int> device_string_id = GetDeviceStringId(form_factor);

  auto format_recent_tabs_promo = [&](int id_with_device,
                                      int id_without_device) {
    return device_string_id.has_value()
               ? l10n_util::GetStringFUTF8(
                     id_with_device,
                     l10n_util::GetStringUTF16(*device_string_id))
               : l10n_util::GetStringUTF8(id_without_device);
  };

  switch (data_type) {
    case syncer::PASSWORDS:
      return format_recent_tabs_promo(
          IDS_ACCOUNT_PREVIEW_RECENT_TABS_PROMO_SUBTITLE_PASSWORDS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_RECENT_TABS_PROMO_SUBTITLE_PASSWORDS);
    case syncer::BOOKMARKS:
      return format_recent_tabs_promo(
          IDS_ACCOUNT_PREVIEW_RECENT_TABS_PROMO_SUBTITLE_BOOKMARKS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_RECENT_TABS_PROMO_SUBTITLE_BOOKMARKS);
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_WALLET_METADATA:
      return format_recent_tabs_promo(
          IDS_ACCOUNT_PREVIEW_RECENT_TABS_PROMO_SUBTITLE_SAVED_INFO_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_RECENT_TABS_PROMO_SUBTITLE_SAVED_INFO);
    case syncer::READING_LIST:
      return format_recent_tabs_promo(
          IDS_ACCOUNT_PREVIEW_RECENT_TABS_PROMO_SUBTITLE_READING_LIST_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_RECENT_TABS_PROMO_SUBTITLE_READING_LIST);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    case syncer::EXTENSIONS:
      // Only allow using extensions data type with generic devices (no specific
      // device) or with Desktop.
      if (device_string_id.has_value() &&
          form_factor !=
              sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP) {
        return std::nullopt;
      }
      return format_recent_tabs_promo(
          IDS_ACCOUNT_PREVIEW_RECENT_TABS_PROMO_SUBTITLE_EXTENSIONS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_RECENT_TABS_PROMO_SUBTITLE_EXTENSIONS);
#endif
    default:
      return std::nullopt;
  }
}

std::optional<std::string> GetProfileSeparationSubtitleForDataType(
    syncer::DataType data_type,
    sync_pb::SyncEnums_DeviceFormFactor form_factor,
    const std::u16string& existing_account_given_name,
    const std::u16string& new_account_email) {
  std::optional<int> device_string_id = GetDeviceStringId(form_factor);

  auto format_separation = [&](int id_with_device, int id_without_device) {
    return device_string_id.has_value()
               ? l10n_util::GetStringFUTF8(
                     id_with_device, existing_account_given_name,
                     new_account_email,
                     l10n_util::GetStringUTF16(*device_string_id))
               : l10n_util::GetStringFUTF8(id_without_device,
                                           existing_account_given_name,
                                           new_account_email);
  };

  switch (data_type) {
    case syncer::PASSWORDS:
      return format_separation(
          IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_PASSWORDS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_PASSWORDS);
    case syncer::BOOKMARKS:
      return format_separation(
          IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_BOOKMARKS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_BOOKMARKS);
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_WALLET_METADATA:
      return format_separation(
          IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_SAVED_INFO_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_SAVED_INFO);
    case syncer::READING_LIST:
      return format_separation(
          IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_READING_LIST_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_READING_LIST);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    case syncer::EXTENSIONS:
      // Only allow using extensions data type with generic devices (no specific
      // device) or with Desktop.
      if (device_string_id.has_value() &&
          form_factor !=
              sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP) {
        return std::nullopt;
      }
      return format_separation(
          IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_EXTENSIONS_WITH_DEVICE,
          IDS_ACCOUNT_PREVIEW_PROFILE_SEPARATION_SUBTITLE_EXTENSIONS);
#endif
    default:
      return std::nullopt;
  }
}

}  // namespace

std::optional<std::string> GetAccountPreviewPromoSubtitle(
    const AccountPreviewDataService::AccountPreviewPreference& preference) {
  for (const auto& preferred_data_type : preference.preferred_data_types) {
    if (auto subtitle =
            GetPromoSubtitleForDataType(preferred_data_type.data_type,
                                        preference.other_device_form_factor)) {
      return subtitle;
    }
  }

  return std::nullopt;
}

std::optional<std::string> GetAccountPreviewProfileMenuSubtitle(
    std::string_view email,
    const AccountPreviewDataService::AccountPreviewPreference& preference) {
  std::u16string email_u16 = base::UTF8ToUTF16(email);

  for (const auto& preferred_data_type : preference.preferred_data_types) {
    if (auto subtitle = GetProfileMenuSubtitleForDataType(
            preferred_data_type.data_type, preference.other_device_form_factor,
            email_u16)) {
      return subtitle;
    }
  }

  return std::nullopt;
}

std::optional<std::string> GetAccountPreviewSettingsPromoSubtitle(
    const AccountPreviewDataService::AccountPreviewPreference& preference) {
  for (const auto& preferred_data_type : preference.preferred_data_types) {
    if (auto subtitle = GetSettingsPromoSubtitleForDataType(
            preferred_data_type.data_type,
            preference.other_device_form_factor)) {
      return subtitle;
    }
  }

  return std::nullopt;
}

std::string GetAccountPreviewHistorySignedInPromoSubtitle(
    const AccountPreviewDataService::AccountPreviewPreference& preference) {
  std::optional<int> device_string_id =
      GetDeviceStringId(preference.other_device_form_factor);
  return device_string_id.has_value()
             ? l10n_util::GetStringFUTF8(
                   IDS_ACCOUNT_PREVIEW_HISTORY_SIGNED_IN_HISTORY_SYNC_PROMO_SUBTITLE_WITH_DEVICE,
                   l10n_util::GetStringUTF16(*device_string_id))
             : l10n_util::GetStringUTF8(
                   IDS_ACCOUNT_PREVIEW_HISTORY_SIGNED_IN_HISTORY_SYNC_PROMO_SUBTITLE);
}

std::optional<std::string> GetAccountPreviewHistoryPromoSubtitle(
    std::string_view email,
    const AccountPreviewDataService::AccountPreviewPreference& preference) {
  std::u16string email_u16 = base::UTF8ToUTF16(email);

  for (const auto& preferred_data_type : preference.preferred_data_types) {
    if (auto subtitle = GetHistoryPromoSubtitleForDataType(
            preferred_data_type.data_type, preference.other_device_form_factor,
            email_u16)) {
      return subtitle;
    }
  }

  return std::nullopt;
}

std::string GetAccountPreviewRecentTabsSignedInPromoSubtitle(
    std::string_view email,
    const AccountPreviewDataService::AccountPreviewPreference& preference) {
  std::u16string email_u16 = base::UTF8ToUTF16(email);
  std::optional<int> device_string_id =
      GetDeviceStringId(preference.other_device_form_factor);
  return device_string_id.has_value()
             ? l10n_util::GetStringFUTF8(
                   IDS_ACCOUNT_PREVIEW_RECENT_TABS_SIGNED_IN_PROMO_SUBTITLE_WITH_DEVICE,
                   l10n_util::GetStringUTF16(*device_string_id), email_u16)
             : l10n_util::GetStringFUTF8(
                   IDS_ACCOUNT_PREVIEW_RECENT_TABS_SIGNED_IN_PROMO_SUBTITLE,
                   email_u16);
}

std::optional<std::string> GetAccountPreviewRecentTabsPromoSubtitle(
    const AccountPreviewDataService::AccountPreviewPreference& preference) {
  for (const auto& preferred_data_type : preference.preferred_data_types) {
    if (auto subtitle = GetRecentTabsPromoSubtitleForDataType(
            preferred_data_type.data_type,
            preference.other_device_form_factor)) {
      return subtitle;
    }
  }

  return std::nullopt;
}

std::optional<std::string> GetAccountPreviewProfileSeparationSubtitle(
    std::string_view existing_account_given_name,
    std::string_view new_account_email,
    const AccountPreviewDataService::AccountPreviewPreference& preference) {
  std::u16string existing_user = base::UTF8ToUTF16(existing_account_given_name);
  std::u16string new_user_email_u16 = base::UTF8ToUTF16(new_account_email);

  for (const auto& preferred_data_type : preference.preferred_data_types) {
    if (auto subtitle = GetProfileSeparationSubtitleForDataType(
            preferred_data_type.data_type, preference.other_device_form_factor,
            existing_user, new_user_email_u16)) {
      return subtitle;
    }
  }

  return std::nullopt;
}

}  // namespace signin
