// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PROTOCOL_PROTO_ENUM_CONVERSIONS_H_
#define COMPONENTS_SYNC_PROTOCOL_PROTO_ENUM_CONVERSIONS_H_

#include "components/sync/protocol/app_list_specifics.pb.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/contact_info_specifics.pb.h"
#include "components/sync/protocol/cookie_specifics.pb.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/gaia_password_reuse.pb.h"
#include "components/sync/protocol/get_updates_caller_info.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/protocol/note_entity.pb.h"
#include "components/sync/protocol/power_bookmark_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/reading_list_specifics.pb.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "components/sync/protocol/sharing_message_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/protocol/user_consent_types.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/sync/protocol/wifi_configuration_specifics.pb.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"

// Keep this file in sync with the .proto files in this directory.
//
// Utility functions to get the string equivalent for some sync proto
// enums.

namespace syncer {

// The returned strings (which don't have to be freed) are in ASCII.
// The result of passing in an invalid enum value is undefined.

const char* ProtoEnumToString(
    sync_pb::AppListSpecifics::AppListItemType item_type);

const char* ProtoEnumToString(sync_pb::AppSpecifics::LaunchType launch_type);

const char* ProtoEnumToString(
    sync_pb::AutofillWalletSpecifics::WalletInfoType wallet_info_type);

const char* ProtoEnumToString(
    sync_pb::BankAccountDetails::AccountType account_type);

const char* ProtoEnumToString(sync_pb::BookmarkSpecifics::Type type);

const char* ProtoEnumToString(
    sync_pb::CommitResponse::ResponseType response_type);

const char* ProtoEnumToString(
    sync_pb::ContactInfoSpecifics::AddressType address_type);

const char* ProtoEnumToString(
    sync_pb::ContactInfoSpecifics::VerificationStatus verification_status);

const char* ProtoEnumToString(
    sync_pb::TrustedVaultAutoUpgradeExperimentGroup::Type type);

const char* ProtoEnumToString(sync_pb::NigoriSpecifics::PassphraseType type);

const char* ProtoEnumToString(
    sync_pb::PaymentInstrument::SupportedRail supported_rail);

const char* ProtoEnumToString(
    sync_pb::PowerBookmarkSpecifics::PowerType power_type);

const char* ProtoEnumToString(sync_pb::NoteEntity::TargetType target_type);

const char* ProtoEnumToString(
    sync_pb::ReadingListSpecifics::ReadingListEntryStatus status);

const char* ProtoEnumToString(sync_pb::SavedTabGroup::SavedTabGroupColor color);

const char* ProtoEnumToString(sync_pb::SharedTabGroup::Color color);

const char* ProtoEnumToString(
    sync_pb::SearchEngineSpecifics::ActiveStatus is_active);

const char* ProtoEnumToString(sync_pb::SessionTab::FaviconType favicon_type);

const char* ProtoEnumToString(sync_pb::SyncEnums::BrowserType browser_type);

const char* ProtoEnumToString(sync_pb::SyncEnums::Action action);

const char* ProtoEnumToString(sync_pb::SyncEnums::DeviceType device_type);

const char* ProtoEnumToString(sync_pb::SyncEnums::OsType os_type);

const char* ProtoEnumToString(
    sync_pb::SyncEnums::DeviceFormFactor device_form_factor);

const char* ProtoEnumToString(sync_pb::SyncEnums::ErrorType error_type);

const char* ProtoEnumToString(sync_pb::SyncEnums::GetUpdatesOrigin origin);

const char* ProtoEnumToString(
    sync_pb::SyncEnums::PageTransition page_transition);

const char* ProtoEnumToString(
    sync_pb::SyncEnums::PageTransitionRedirectType redirect_type);

const char* ProtoEnumToString(
    sync_pb::SyncEnums::SendTabReceivingType send_tab_receiving_type);

const char* ProtoEnumToString(sync_pb::SyncEnums::SingletonDebugEventType type);

const char* ProtoEnumToString(sync_pb::TabNavigation::BlockedState state);

const char* ProtoEnumToString(sync_pb::SyncEnums::PasswordState state);

const char* ProtoEnumToString(sync_pb::UserConsentTypes::ConsentStatus status);

const char* ProtoEnumToString(
    sync_pb::GaiaPasswordReuse::PasswordReuseDetected::SafeBrowsingStatus::
        ReportingPopulation safe_browsing_reporting_population);

const char* ProtoEnumToString(
    sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction::
        InteractionResult interaction_result);

const char* ProtoEnumToString(
    sync_pb::GaiaPasswordReuse::PasswordReuseLookup::LookupResult
        lookup_result);

const char* ProtoEnumToString(
    sync_pb::GaiaPasswordReuse::PasswordReuseLookup::ReputationVerdict verdict);

const char* ProtoEnumToString(
    sync_pb::UserEventSpecifics::GaiaPasswordCaptured::EventTrigger trigger);

const char* ProtoEnumToString(
    sync_pb::UserEventSpecifics::FlocIdComputed::EventTrigger trigger);

const char* ProtoEnumToString(
    sync_pb::WalletMaskedCreditCard::CardInfoRetrievalEnrollmentState
        card_info_retrieval_enrollment_state);

const char* ProtoEnumToString(
    sync_pb::WalletMaskedCreditCard::VirtualCardEnrollmentState
        virtual_card_enrollment_state);

const char* ProtoEnumToString(
    sync_pb::WalletMaskedCreditCard::VirtualCardEnrollmentType
        virtual_card_enrollment_type);

const char* ProtoEnumToString(
    sync_pb::WalletMaskedCreditCard::WalletCardStatus wallet_card_status);

const char* ProtoEnumToString(
    sync_pb::WalletMaskedCreditCard::WalletCardType wallet_card_type);

const char* ProtoEnumToString(
    sync_pb::CardBenefit::CategoryBenefitType category_benefit_type);

const char* ProtoEnumToString(sync_pb::CardIssuer::Issuer issuer);

const char* ProtoEnumToString(
    sync_pb::WalletMetadataSpecifics::Type wallet_metadata_type);

const char* ProtoEnumToString(sync_pb::WebApkIconInfo::Purpose purpose);

const char* ProtoEnumToString(sync_pb::WebAppIconInfo::Purpose purpose);

const char* ProtoEnumToString(
    sync_pb::WebAppSpecifics::UserDisplayMode user_display_mode);

const char* ProtoEnumToString(
    sync_pb::AutofillProfileSpecifics::VerificationStatus status);

const char* ProtoEnumToString(
    sync_pb::WifiConfigurationSpecifics::SecurityType security_type);

const char* ProtoEnumToString(
    sync_pb::WifiConfigurationSpecifics::AutomaticallyConnectOption
        automatically_connect_option);

const char* ProtoEnumToString(
    sync_pb::WifiConfigurationSpecifics::IsPreferredOption is_preferred_option);

const char* ProtoEnumToString(
    sync_pb::WifiConfigurationSpecifics::MeteredOption metered_option);

const char* ProtoEnumToString(
    sync_pb::WifiConfigurationSpecifics::ProxyConfiguration::ProxyOption
        proxy_option);

const char* ProtoEnumToString(
    sync_pb::WorkspaceDeskSpecifics::WindowState window_state);

const char* ProtoEnumToString(
    sync_pb::WorkspaceDeskSpecifics::LaunchContainer container);

const char* ProtoEnumToString(
    sync_pb::WorkspaceDeskSpecifics::WindowOpenDisposition disposition);

const char* ProtoEnumToString(
    sync_pb::UserConsentTypes::AssistantActivityControlConsent::SettingType
        setting_type);

const char* ProtoEnumToString(sync_pb::WorkspaceDeskSpecifics::DeskType type);

const char* ProtoEnumToString(
    sync_pb::WorkspaceDeskSpecifics::TabGroupColor color);

const char* ProtoEnumToString(sync_pb::DataTypeState::InitialSyncState state);

const char* ProtoEnumToString(
    sync_pb::CookieSpecifics::CookieSameSite site_restrictions);

const char* ProtoEnumToString(
    sync_pb::CookieSpecifics::CookiePriority priority);

const char* ProtoEnumToString(
    sync_pb::CookieSpecifics::CookieSourceScheme source_scheme);

const char* ProtoEnumToString(
    sync_pb::CookieSpecifics::CookieSourceType source_type);

const char* ProtoEnumToString(
    sync_pb::SharingMessageSpecifics::ChannelConfiguration::
        ChimeChannelConfiguration::ChimeChannelType channel_type);

const char* ProtoEnumToString(
    sync_pb::ThemeSpecifics::UserColorTheme::BrowserColorVariant
        browser_color_variant);

const char* ProtoEnumToString(
    sync_pb::ThemeSpecifics::BrowserColorScheme browser_color_scheme);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_PROTOCOL_PROTO_ENUM_CONVERSIONS_H_
