// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PROTOCOL_PROTO_VALUE_CONVERSIONS_H_
#define COMPONENTS_SYNC_PROTOCOL_PROTO_VALUE_CONVERSIONS_H_

#include <memory>

namespace base {
class DictionaryValue;
}

namespace sync_pb {
class AppListSpecifics;
class AppNotificationSettings;
class AppSettingSpecifics;
class AppSpecifics;
class ArcPackageSpecifics;
class AutofillProfileSpecifics;
class AutofillSpecifics;
class AutofillWalletSpecifics;
class BookmarkSpecifics;
class ClientConfigParams;
class ClientToServerMessage;
class ClientToServerResponse;
class DatatypeAssociationStats;
class DebugEventInfo;
class DebugInfo;
class DeviceInfoSpecifics;
class DictionarySpecifics;
class EncryptedData;
class EntityMetadata;
class EntitySpecifics;
class ExperimentsSpecifics;
class ExtensionSettingSpecifics;
class ExtensionSpecifics;
class FaviconImageSpecifics;
class FaviconTrackingSpecifics;
class HistoryDeleteDirectiveSpecifics;
class LinkedAppIconInfo;
class ManagedUserSettingSpecifics;
class ManagedUserWhitelistSpecifics;
class NavigationRedirect;
class NigoriSpecifics;
class OsPreferenceSpecifics;
class OsPriorityPreferenceSpecifics;
class PasswordSpecifics;
class PasswordSpecificsData;
class PaymentsCustomerData;
class PreferenceSpecifics;
class PrinterPPDReference;
class PrinterSpecifics;
class PriorityPreferenceSpecifics;
class ReadingListSpecifics;
class SearchEngineSpecifics;
class SecurityEventSpecifics;
class SendTabToSelfSpecifics;
class SessionHeader;
class SessionSpecifics;
class SessionTab;
class SessionWindow;
class SyncCycleCompletedEventInfo;
class SyncEntity;
class TabNavigation;
class ThemeSpecifics;
class TimeRangeDirective;
class TypedUrlSpecifics;
class UrlDirective;
class UserConsentSpecifics;
class UserEventSpecifics;
class WalletCreditCardCloudTokenData;
class WalletMaskedCreditCard;
class WalletMetadataSpecifics;
class WalletPostalAddress;
class WebAppSpecifics;
class WifiConfigurationSpecifics;
}  // namespace sync_pb

// Keep this file in sync with the .proto files in this directory.
//
// Utility functions to convert sync protocol buffers to dictionaries.
// Each protocol field is mapped to a key of the same name.  Repeated
// fields are mapped to array values and sub-messages are mapped to
// sub-dictionary values.

namespace syncer {

std::unique_ptr<base::DictionaryValue> AppListSpecificsToValue(
    const sync_pb::AppListSpecifics& proto);

std::unique_ptr<base::DictionaryValue> AppNotificationSettingsToValue(
    const sync_pb::AppNotificationSettings& app_notification_settings);

std::unique_ptr<base::DictionaryValue> AppSettingSpecificsToValue(
    const sync_pb::AppSettingSpecifics& app_setting_specifics);

std::unique_ptr<base::DictionaryValue> AppSpecificsToValue(
    const sync_pb::AppSpecifics& app_specifics);

std::unique_ptr<base::DictionaryValue> ArcPackageSpecificsToValue(
    const sync_pb::ArcPackageSpecifics& proto);

std::unique_ptr<base::DictionaryValue> AutofillProfileSpecificsToValue(
    const sync_pb::AutofillProfileSpecifics& autofill_profile_specifics);

std::unique_ptr<base::DictionaryValue> AutofillSpecificsToValue(
    const sync_pb::AutofillSpecifics& autofill_specifics);

std::unique_ptr<base::DictionaryValue> AutofillWalletSpecificsToValue(
    const sync_pb::AutofillWalletSpecifics& autofill_wallet_specifics);

std::unique_ptr<base::DictionaryValue> BookmarkSpecificsToValue(
    const sync_pb::BookmarkSpecifics& bookmark_specifics);

std::unique_ptr<base::DictionaryValue> ClientConfigParamsToValue(
    const sync_pb::ClientConfigParams& proto);

std::unique_ptr<base::DictionaryValue> DatatypeAssociationStatsToValue(
    const sync_pb::DatatypeAssociationStats& proto);

std::unique_ptr<base::DictionaryValue> DebugEventInfoToValue(
    const sync_pb::DebugEventInfo& proto);

std::unique_ptr<base::DictionaryValue> DebugInfoToValue(
    const sync_pb::DebugInfo& proto);

std::unique_ptr<base::DictionaryValue> DeviceInfoSpecificsToValue(
    const sync_pb::DeviceInfoSpecifics& device_info_specifics);

std::unique_ptr<base::DictionaryValue> DictionarySpecificsToValue(
    const sync_pb::DictionarySpecifics& dictionary_specifics);

std::unique_ptr<base::DictionaryValue> EncryptedDataToValue(
    const sync_pb::EncryptedData& encrypted_data);

std::unique_ptr<base::DictionaryValue> EntityMetadataToValue(
    const sync_pb::EntityMetadata& metadata);

std::unique_ptr<base::DictionaryValue> EntitySpecificsToValue(
    const sync_pb::EntitySpecifics& specifics);

std::unique_ptr<base::DictionaryValue> ExperimentsSpecificsToValue(
    const sync_pb::ExperimentsSpecifics& proto);

std::unique_ptr<base::DictionaryValue> ExtensionSettingSpecificsToValue(
    const sync_pb::ExtensionSettingSpecifics& extension_setting_specifics);

std::unique_ptr<base::DictionaryValue> ExtensionSpecificsToValue(
    const sync_pb::ExtensionSpecifics& extension_specifics);

std::unique_ptr<base::DictionaryValue> FaviconImageSpecificsToValue(
    const sync_pb::FaviconImageSpecifics& favicon_image_specifics);

std::unique_ptr<base::DictionaryValue> FaviconTrackingSpecificsToValue(
    const sync_pb::FaviconTrackingSpecifics& favicon_tracking_specifics);

std::unique_ptr<base::DictionaryValue> HistoryDeleteDirectiveSpecificsToValue(
    const sync_pb::HistoryDeleteDirectiveSpecifics&
        history_delete_directive_specifics);

std::unique_ptr<base::DictionaryValue> LinkedAppIconInfoToValue(
    const sync_pb::LinkedAppIconInfo& linked_app_icon_info);

std::unique_ptr<base::DictionaryValue> ManagedUserSettingSpecificsToValue(
    const sync_pb::ManagedUserSettingSpecifics& managed_user_setting_specifics);

std::unique_ptr<base::DictionaryValue> ManagedUserWhitelistSpecificsToValue(
    const sync_pb::ManagedUserWhitelistSpecifics&
        managed_user_whitelist_specifics);

std::unique_ptr<base::DictionaryValue> NavigationRedirectToValue(
    const sync_pb::NavigationRedirect& navigation_redirect);

std::unique_ptr<base::DictionaryValue> NigoriSpecificsToValue(
    const sync_pb::NigoriSpecifics& nigori_specifics);

std::unique_ptr<base::DictionaryValue> OsPreferenceSpecificsToValue(
    const sync_pb::OsPreferenceSpecifics& specifics);

std::unique_ptr<base::DictionaryValue> OsPriorityPreferenceSpecificsToValue(
    const sync_pb::OsPriorityPreferenceSpecifics& specifics);

std::unique_ptr<base::DictionaryValue> PasswordSpecificsToValue(
    const sync_pb::PasswordSpecifics& password_specifics);

std::unique_ptr<base::DictionaryValue> PasswordSpecificsDataToValue(
    const sync_pb::PasswordSpecificsData& password_specifics_data);

std::unique_ptr<base::DictionaryValue> PaymentsCustomerDataToValue(
    const sync_pb::PaymentsCustomerData& payments_customer_data);

std::unique_ptr<base::DictionaryValue> PreferenceSpecificsToValue(
    const sync_pb::PreferenceSpecifics& password_specifics);

std::unique_ptr<base::DictionaryValue> PrinterPPDReferenceToValue(
    const sync_pb::PrinterPPDReference& proto);

std::unique_ptr<base::DictionaryValue> PrinterSpecificsToValue(
    const sync_pb::PrinterSpecifics& printer_specifics);

std::unique_ptr<base::DictionaryValue> PriorityPreferenceSpecificsToValue(
    const sync_pb::PriorityPreferenceSpecifics& proto);

std::unique_ptr<base::DictionaryValue> ReadingListSpecificsToValue(
    const sync_pb::ReadingListSpecifics& proto);

std::unique_ptr<base::DictionaryValue> SearchEngineSpecificsToValue(
    const sync_pb::SearchEngineSpecifics& search_engine_specifics);

std::unique_ptr<base::DictionaryValue> SendTabToSelfSpecificsToValue(
    const sync_pb::SendTabToSelfSpecifics& send_tab_specifics);

std::unique_ptr<base::DictionaryValue> SecurityEventSpecificsToValue(
    const sync_pb::SecurityEventSpecifics& security_event_specifics);

std::unique_ptr<base::DictionaryValue> SessionHeaderToValue(
    const sync_pb::SessionHeader& session_header);

std::unique_ptr<base::DictionaryValue> SessionSpecificsToValue(
    const sync_pb::SessionSpecifics& session_specifics);

std::unique_ptr<base::DictionaryValue> SessionTabToValue(
    const sync_pb::SessionTab& session_tab);

std::unique_ptr<base::DictionaryValue> SessionWindowToValue(
    const sync_pb::SessionWindow& session_window);

std::unique_ptr<base::DictionaryValue> SyncCycleCompletedEventInfoToValue(
    const sync_pb::SyncCycleCompletedEventInfo& proto);

std::unique_ptr<base::DictionaryValue> TabNavigationToValue(
    const sync_pb::TabNavigation& tab_navigation);

std::unique_ptr<base::DictionaryValue> ThemeSpecificsToValue(
    const sync_pb::ThemeSpecifics& theme_specifics);

std::unique_ptr<base::DictionaryValue> TimeRangeDirectiveToValue(
    const sync_pb::TimeRangeDirective& time_range_directive);

std::unique_ptr<base::DictionaryValue> TypedUrlSpecificsToValue(
    const sync_pb::TypedUrlSpecifics& typed_url_specifics);

std::unique_ptr<base::DictionaryValue> UrlDirectiveToValue(
    const sync_pb::UrlDirective& time_range_directive);

std::unique_ptr<base::DictionaryValue> UserConsentSpecificsToValue(
    const sync_pb::UserConsentSpecifics& user_consent_specifics);

std::unique_ptr<base::DictionaryValue> UserEventSpecificsToValue(
    const sync_pb::UserEventSpecifics& user_event_specifics);

std::unique_ptr<base::DictionaryValue> WalletCreditCardCloudTokenDataToValue(
    const sync_pb::WalletCreditCardCloudTokenData& cloud_token_data);

std::unique_ptr<base::DictionaryValue> WalletMaskedCreditCardToValue(
    const sync_pb::WalletMaskedCreditCard& wallet_masked_card);

std::unique_ptr<base::DictionaryValue> WalletMetadataSpecificsToValue(
    const sync_pb::WalletMetadataSpecifics& wallet_metadata_specifics);

std::unique_ptr<base::DictionaryValue> WalletPostalAddressToValue(
    const sync_pb::WalletPostalAddress& wallet_postal_address);

std::unique_ptr<base::DictionaryValue> WebAppSpecificsToValue(
    const sync_pb::WebAppSpecifics& web_app_specifics);

std::unique_ptr<base::DictionaryValue> WifiConfigurationSpecificsToValue(
    const sync_pb::WifiConfigurationSpecifics& wifi_configuration_specifics);

// ToValue functions that allow omitting specifics.

std::unique_ptr<base::DictionaryValue> ClientToServerMessageToValue(
    const sync_pb::ClientToServerMessage& proto,
    bool include_specifics);

std::unique_ptr<base::DictionaryValue> ClientToServerResponseToValue(
    const sync_pb::ClientToServerResponse& proto,
    bool include_specifics);

std::unique_ptr<base::DictionaryValue> SyncEntityToValue(
    const sync_pb::SyncEntity& entity,
    bool include_specifics);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_PROTOCOL_PROTO_VALUE_CONVERSIONS_H_
