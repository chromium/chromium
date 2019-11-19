// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PROTOCOL_PROTO_ENUM_CONVERSIONS_H_
#define COMPONENTS_SYNC_PROTOCOL_PROTO_ENUM_CONVERSIONS_H_

#include "components/sync/protocol/app_list_specifics.pb.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/client_debug_info.pb.h"
#include "components/sync/protocol/reading_list_specifics.pb.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/web_app_specifics.pb.h"

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
    sync_pb::CommitResponse::ResponseType response_type);

const char* ProtoEnumToString(
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource updates_source);

const char* ProtoEnumToString(sync_pb::NigoriSpecifics::PassphraseType type);

const char* ProtoEnumToString(
    sync_pb::ReadingListSpecifics::ReadingListEntryStatus status);

const char* ProtoEnumToString(sync_pb::SessionTab::FaviconType favicon_type);

const char* ProtoEnumToString(sync_pb::SessionWindow::BrowserType browser_type);

const char* ProtoEnumToString(sync_pb::SyncEnums::Action action);

const char* ProtoEnumToString(sync_pb::SyncEnums::DeviceType device_type);

const char* ProtoEnumToString(sync_pb::SyncEnums::ErrorType error_type);

const char* ProtoEnumToString(sync_pb::SyncEnums::GetUpdatesOrigin origin);

const char* ProtoEnumToString(
    sync_pb::SyncEnums::PageTransition page_transition);

const char* ProtoEnumToString(
    sync_pb::SyncEnums::PageTransitionRedirectType redirect_type);

const char* ProtoEnumToString(sync_pb::SyncEnums::SingletonDebugEventType type);

const char* ProtoEnumToString(sync_pb::TabNavigation::BlockedState state);

const char* ProtoEnumToString(sync_pb::TabNavigation::PasswordState state);

const char* ProtoEnumToString(sync_pb::UserConsentSpecifics::Feature feature);

const char* ProtoEnumToString(
    sync_pb::UserEventSpecifics::Translation::Interaction interaction);

const char* ProtoEnumToString(
    sync_pb::UserEventSpecifics::UserConsent::Feature feature);

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

// TODO(markusheintz): Remove.
const char* ProtoEnumToString(
    sync_pb::GaiaPasswordReuse::PasswordCaptured::EventTrigger trigger);

const char* ProtoEnumToString(
    sync_pb::UserEventSpecifics::GaiaPasswordCaptured::EventTrigger trigger);

const char* ProtoEnumToString(
    sync_pb::WalletMaskedCreditCard::WalletCardClass wallet_card_class);

const char* ProtoEnumToString(
    sync_pb::WalletMaskedCreditCard::WalletCardStatus wallet_card_status);

const char* ProtoEnumToString(
    sync_pb::WalletMaskedCreditCard::WalletCardType wallet_card_type);

const char* ProtoEnumToString(
    sync_pb::WalletMetadataSpecifics::Type wallet_metadata_type);

const char* ProtoEnumToString(
    sync_pb::WebAppSpecifics::UserDisplayMode user_display_mode);

const char* ProtoEnumToString(
    sync_pb::WifiConfigurationSpecificsData::SecurityType security_type);

const char* ProtoEnumToString(
    sync_pb::WifiConfigurationSpecificsData::AutomaticallyConnectOption
        automatically_connect_option);

const char* ProtoEnumToString(
    sync_pb::WifiConfigurationSpecificsData::IsPreferredOption
        is_preferred_option);

const char* ProtoEnumToString(
    sync_pb::WifiConfigurationSpecificsData::MeteredOption metered_option);

const char* ProtoEnumToString(
    sync_pb::WifiConfigurationSpecificsData::ProxyConfiguration::ProxyOption
        proxy_option);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_PROTOCOL_PROTO_ENUM_CONVERSIONS_H_
