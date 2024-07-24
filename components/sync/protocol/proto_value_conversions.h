// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PROTOCOL_PROTO_VALUE_CONVERSIONS_H_
#define COMPONENTS_SYNC_PROTOCOL_PROTO_VALUE_CONVERSIONS_H_

namespace base {
class Value;
}

namespace sync_pb {
class AppListSpecifics;
class AppSettingSpecifics;
class AppSpecifics;
class ArcPackageSpecifics;
class AutofillProfileSpecifics;
class AutofillSpecifics;
class AutofillOfferSpecifics;
class AutofillWalletCredentialSpecifics;
class AutofillWalletSpecifics;
class AutofillWalletUsageSpecifics;
class BankAccountDetails;
class BookmarkSpecifics;
class ClientConfigParams;
class ClientToServerMessage;
class ClientToServerResponse;
class CollaborationGroupSpecifics;
class ContactInfoSpecifics;
class CookieSpecifics;
class CrossUserSharingPublicKey;
class DebugEventInfo;
class DebugInfo;
class DeviceDetails;
class DeviceInfoSpecifics;
class DictionarySpecifics;
class EncryptedData;
class EntityMetadata;
class EntitySpecifics;
class EwalletDetails;
class ExtensionSettingSpecifics;
class ExtensionSpecifics;
class HistoryDeleteDirectiveSpecifics;
class HistorySpecifics;
class IncomingPasswordSharingInvitationSpecifics;
class LinkedAppIconInfo;
class ManagedUserSettingSpecifics;
class NavigationRedirect;
class NigoriSpecifics;
class OsPreferenceSpecifics;
class OsPriorityPreferenceSpecifics;
class OutgoingPasswordSharingInvitationSpecifics;
class PasswordSpecifics;
class PasswordSpecificsData;
class PaymentInstrument;
class PaymentsCustomerData;
class PlusAddressSettingSpecifics;
class PlusAddressSpecifics;
class PowerBookmarkSpecifics;
class PreferenceSpecifics;
class PrinterPPDReference;
class PrinterSpecifics;
class PrintersAuthorizationServerSpecifics;
class PriorityPreferenceSpecifics;
class ProductComparisonSpecifics;
class ReadingListSpecifics;
class SavedTabGroupSpecifics;
class SearchEngineSpecifics;
class SecurityEventSpecifics;
class SendTabToSelfPush;
class SendTabToSelfSpecifics;
class SessionHeader;
class SessionSpecifics;
class SessionTab;
class SessionWindow;
class SharingMessageSpecifics;
class SyncCycleCompletedEventInfo;
class SyncEntity;
class TabNavigation;
class ThemeSpecifics;
class TimeRangeDirective;
class TypedUrlSpecifics;
class UnencryptedSharingMessage;
class UrlDirective;
class UserConsentSpecifics;
class UserEventSpecifics;
class WalletCreditCardCloudTokenData;
class WalletMaskedCreditCard;
class WalletMetadataSpecifics;
class WalletPostalAddress;
class WebApkSpecifics;
class WebAppSpecifics;
class WebauthnCredentialSpecifics;
class WifiConfigurationSpecifics;
class WorkspaceDeskSpecifics;
}  // namespace sync_pb

// Keep this file in sync with the .proto files in this directory.
//
// Utility functions to convert sync protocol buffers to dictionaries.
// Each protocol field is mapped to a key of the same name.  Repeated
// fields are mapped to array values and sub-messages are mapped to
// sub-dictionary values.

namespace syncer {

base::Value AppListSpecificsToValue(const sync_pb::AppListSpecifics& proto);

base::Value AppSettingSpecificsToValue(
    const sync_pb::AppSettingSpecifics& app_setting_specifics);

base::Value AppSpecificsToValue(const sync_pb::AppSpecifics& app_specifics);

base::Value ArcPackageSpecificsToValue(
    const sync_pb::ArcPackageSpecifics& proto);

base::Value AutofillOfferSpecificsToValue(
    const sync_pb::AutofillOfferSpecifics& autofill_offer_specifics);

base::Value AutofillProfileSpecificsToValue(
    const sync_pb::AutofillProfileSpecifics& autofill_profile_specifics);

base::Value AutofillSpecificsToValue(
    const sync_pb::AutofillSpecifics& autofill_specifics);

base::Value AutofillWalletCredentialSpecificsToValue(
    const sync_pb::AutofillWalletCredentialSpecifics&
        autofill_wallet_credential_specifics);

base::Value AutofillWalletSpecificsToValue(
    const sync_pb::AutofillWalletSpecifics& autofill_wallet_specifics);

base::Value AutofillWalletUsageSpecificsToValue(
    const sync_pb::AutofillWalletUsageSpecifics&
        autofill_wallet_usage_specifics);

base::Value BankAccountDetailsToValue(
    const sync_pb::BankAccountDetails& bank_account_details);

base::Value BookmarkSpecificsToValue(
    const sync_pb::BookmarkSpecifics& bookmark_specifics);

base::Value ClientConfigParamsToValue(const sync_pb::ClientConfigParams& proto);

base::Value CollaborationGroupSpecificsToValue(
    const sync_pb::CollaborationGroupSpecifics& proto);

base::Value ContactInfoSpecificsToValue(
    const sync_pb::ContactInfoSpecifics& proto);

base::Value CookieSpecificsToValue(const sync_pb::CookieSpecifics& proto);

base::Value DebugEventInfoToValue(const sync_pb::DebugEventInfo& proto);

base::Value DebugInfoToValue(const sync_pb::DebugInfo& proto);

base::Value DeviceDetailsToValue(const sync_pb::DeviceDetails& device_details);

base::Value DeviceInfoSpecificsToValue(
    const sync_pb::DeviceInfoSpecifics& device_info_specifics);

base::Value DictionarySpecificsToValue(
    const sync_pb::DictionarySpecifics& dictionary_specifics);

base::Value EncryptedDataToValue(const sync_pb::EncryptedData& encrypted_data);

base::Value EntityMetadataToValue(const sync_pb::EntityMetadata& metadata);

base::Value EntitySpecificsToValue(const sync_pb::EntitySpecifics& specifics);

base::Value EwalletDetailsToValue(
    const sync_pb::EwalletDetails& ewallet_details);

base::Value ExtensionSettingSpecificsToValue(
    const sync_pb::ExtensionSettingSpecifics& extension_setting_specifics);

base::Value ExtensionSpecificsToValue(
    const sync_pb::ExtensionSpecifics& extension_specifics);

base::Value HistoryDeleteDirectiveSpecificsToValue(
    const sync_pb::HistoryDeleteDirectiveSpecifics&
        history_delete_directive_specifics);

base::Value HistorySpecificsToValue(
    const sync_pb::HistorySpecifics& history_specifics);

base::Value IncomingPasswordSharingInvitationSpecificsToValue(
    const sync_pb::IncomingPasswordSharingInvitationSpecifics& specifics);

base::Value LinkedAppIconInfoToValue(
    const sync_pb::LinkedAppIconInfo& linked_app_icon_info);

base::Value ManagedUserSettingSpecificsToValue(
    const sync_pb::ManagedUserSettingSpecifics& managed_user_setting_specifics);

base::Value NavigationRedirectToValue(
    const sync_pb::NavigationRedirect& navigation_redirect);

base::Value NigoriSpecificsToValue(
    const sync_pb::NigoriSpecifics& nigori_specifics);

base::Value OsPreferenceSpecificsToValue(
    const sync_pb::OsPreferenceSpecifics& specifics);

base::Value OsPriorityPreferenceSpecificsToValue(
    const sync_pb::OsPriorityPreferenceSpecifics& specifics);

base::Value OutgoingPasswordSharingInvitationSpecificsToValue(
    const sync_pb::OutgoingPasswordSharingInvitationSpecifics& specifics);

base::Value PasswordSpecificsToValue(
    const sync_pb::PasswordSpecifics& password_specifics);

base::Value PasswordSpecificsDataToValue(
    const sync_pb::PasswordSpecificsData& password_specifics_data);

base::Value PaymentInstrumentToValue(
    const sync_pb::PaymentInstrument& payment_instrument);

base::Value PaymentsCustomerDataToValue(
    const sync_pb::PaymentsCustomerData& payments_customer_data);

base::Value PlusAddressSettingSpecificsToValue(
    const sync_pb::PlusAddressSettingSpecifics& plus_address_setting_specifics);

base::Value PlusAddressSpecificsToValue(
    const sync_pb::PlusAddressSpecifics& plus_address_specifics);

base::Value PowerBookmarkSpecificsToValue(
    const sync_pb::PowerBookmarkSpecifics& power_bookmark_specifics);

base::Value PreferenceSpecificsToValue(
    const sync_pb::PreferenceSpecifics& password_specifics);

base::Value PrinterPPDReferenceToValue(
    const sync_pb::PrinterPPDReference& proto);

base::Value PrinterSpecificsToValue(
    const sync_pb::PrinterSpecifics& printer_specifics);

base::Value PrintersAuthorizationServerSpecificsToValue(
    const sync_pb::PrintersAuthorizationServerSpecifics&
        printers_authorization_server_specifics);

base::Value PriorityPreferenceSpecificsToValue(
    const sync_pb::PriorityPreferenceSpecifics& proto);

base::Value ProductComparisonSpecificsToValue(
    const sync_pb::ProductComparisonSpecifics& product_comparison_specifics);

base::Value CrossUserSharingPublicKeyToValue(
    const sync_pb::CrossUserSharingPublicKey& proto);

base::Value ReadingListSpecificsToValue(
    const sync_pb::ReadingListSpecifics& proto);

base::Value SavedTabGroupSpecificsToValue(
    const sync_pb::SavedTabGroupSpecifics& saved_tab_group_specifics);

base::Value SearchEngineSpecificsToValue(
    const sync_pb::SearchEngineSpecifics& search_engine_specifics);

base::Value SendTabToSelfPushToValue(
    const sync_pb::SendTabToSelfPush& send_tab_push);

base::Value SendTabToSelfSpecificsToValue(
    const sync_pb::SendTabToSelfSpecifics& send_tab_specifics);

base::Value SecurityEventSpecificsToValue(
    const sync_pb::SecurityEventSpecifics& security_event_specifics);

base::Value SessionHeaderToValue(const sync_pb::SessionHeader& session_header);

base::Value SessionSpecificsToValue(
    const sync_pb::SessionSpecifics& session_specifics);

base::Value SessionTabToValue(const sync_pb::SessionTab& session_tab);

base::Value SessionWindowToValue(const sync_pb::SessionWindow& session_window);

base::Value SharingMessageSpecificsToValue(
    const sync_pb::SharingMessageSpecifics& sharing_message_specifics);

base::Value SyncCycleCompletedEventInfoToValue(
    const sync_pb::SyncCycleCompletedEventInfo& proto);

base::Value TabNavigationToValue(const sync_pb::TabNavigation& tab_navigation);

base::Value ThemeSpecificsToValue(
    const sync_pb::ThemeSpecifics& theme_specifics);

base::Value TimeRangeDirectiveToValue(
    const sync_pb::TimeRangeDirective& time_range_directive);

base::Value TypedUrlSpecificsToValue(
    const sync_pb::TypedUrlSpecifics& typed_url_specifics);

base::Value UnencryptedSharingMessageToValue(
    const sync_pb::UnencryptedSharingMessage& proto);

base::Value UrlDirectiveToValue(
    const sync_pb::UrlDirective& time_range_directive);

base::Value UserConsentSpecificsToValue(
    const sync_pb::UserConsentSpecifics& user_consent_specifics);

base::Value UserEventSpecificsToValue(
    const sync_pb::UserEventSpecifics& user_event_specifics);

base::Value WalletCreditCardCloudTokenDataToValue(
    const sync_pb::WalletCreditCardCloudTokenData& cloud_token_data);

base::Value WalletMaskedCreditCardToValue(
    const sync_pb::WalletMaskedCreditCard& wallet_masked_card);

base::Value WalletMetadataSpecificsToValue(
    const sync_pb::WalletMetadataSpecifics& wallet_metadata_specifics);

base::Value WalletPostalAddressToValue(
    const sync_pb::WalletPostalAddress& wallet_postal_address);

base::Value WebApkSpecificsToValue(
    const sync_pb::WebApkSpecifics& web_apk_specifics);

base::Value WebAppSpecificsToValue(
    const sync_pb::WebAppSpecifics& web_app_specifics);

base::Value WebAuthnCredentialSpecificsToValue(
    const sync_pb::WebauthnCredentialSpecifics& webauthn_credential_specifics);

base::Value WifiConfigurationSpecificsToValue(
    const sync_pb::WifiConfigurationSpecifics& wifi_configuration_specifics);

base::Value WorkspaceDeskSpecificsToValue(
    const sync_pb::WorkspaceDeskSpecifics& workspace_desk_specifics);

// ToValue functions that allow omitting specifics and other fields.

struct ProtoValueConversionOptions {
  // Whether to include specifics.
  bool include_specifics = true;

  // Whether to include default values which are set in GetUpdateTriggers.
  bool include_full_get_update_triggers = true;
};

base::Value ClientToServerMessageToValue(
    const sync_pb::ClientToServerMessage& proto,
    const ProtoValueConversionOptions& options);

base::Value ClientToServerResponseToValue(
    const sync_pb::ClientToServerResponse& proto,
    const ProtoValueConversionOptions& options);

base::Value SyncEntityToValue(const sync_pb::SyncEntity& entity,
                              const ProtoValueConversionOptions& options);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_PROTOCOL_PROTO_VALUE_CONVERSIONS_H_
