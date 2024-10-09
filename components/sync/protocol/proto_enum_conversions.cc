// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/proto_enum_conversions.h"

#include "base/logging.h"
#include "base/notreached.h"

namespace syncer {

#define ASSERT_ENUM_BOUNDS(enum_parent, enum_type, enum_min, enum_max) \
  static_assert(enum_parent::enum_type##_MIN == enum_parent::enum_min, \
                #enum_type "_MIN should be " #enum_min);               \
  static_assert(enum_parent::enum_type##_MAX == enum_parent::enum_max, \
                #enum_type "_MAX should be " #enum_max);

#define ENUM_CASE(enum_parent, enum_value) \
  case enum_parent::enum_value:            \
    return #enum_value

const char* ProtoEnumToString(
    sync_pb::AppListSpecifics::AppListItemType item_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::AppListSpecifics, AppListItemType, TYPE_APP,
                     TYPE_PAGE_BREAK);
  switch (item_type) {
    ENUM_CASE(sync_pb::AppListSpecifics, TYPE_APP);
    ENUM_CASE(sync_pb::AppListSpecifics, TYPE_REMOVE_DEFAULT_APP);
    ENUM_CASE(sync_pb::AppListSpecifics, TYPE_FOLDER);
    ENUM_CASE(sync_pb::AppListSpecifics, TYPE_OBSOLETE_URL);
    ENUM_CASE(sync_pb::AppListSpecifics, TYPE_PAGE_BREAK);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::AppSpecifics::LaunchType launch_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::AppSpecifics, LaunchType, PINNED, WINDOW);
  switch (launch_type) {
    ENUM_CASE(sync_pb::AppSpecifics, PINNED);
    ENUM_CASE(sync_pb::AppSpecifics, REGULAR);
    ENUM_CASE(sync_pb::AppSpecifics, FULLSCREEN);
    ENUM_CASE(sync_pb::AppSpecifics, WINDOW);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::AutofillProfileSpecifics::VerificationStatus status) {
  ASSERT_ENUM_BOUNDS(sync_pb::AutofillProfileSpecifics, VerificationStatus,
                     VERIFICATION_STATUS_UNSPECIFIED, SERVER_PARSED);
  switch (status) {
    ENUM_CASE(sync_pb::AutofillProfileSpecifics,
              VERIFICATION_STATUS_UNSPECIFIED);
    ENUM_CASE(sync_pb::AutofillProfileSpecifics, PARSED);
    ENUM_CASE(sync_pb::AutofillProfileSpecifics, FORMATTED);
    ENUM_CASE(sync_pb::AutofillProfileSpecifics, OBSERVED);
    ENUM_CASE(sync_pb::AutofillProfileSpecifics, USER_VERIFIED);
    ENUM_CASE(sync_pb::AutofillProfileSpecifics, SERVER_PARSED);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::AutofillWalletSpecifics::WalletInfoType wallet_info_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::AutofillWalletSpecifics, WalletInfoType, UNKNOWN,
                     MASKED_IBAN);
  switch (wallet_info_type) {
    ENUM_CASE(sync_pb::AutofillWalletSpecifics, UNKNOWN);
    ENUM_CASE(sync_pb::AutofillWalletSpecifics, MASKED_CREDIT_CARD);
    ENUM_CASE(sync_pb::AutofillWalletSpecifics, POSTAL_ADDRESS);
    ENUM_CASE(sync_pb::AutofillWalletSpecifics, CUSTOMER_DATA);
    ENUM_CASE(sync_pb::AutofillWalletSpecifics, CREDIT_CARD_CLOUD_TOKEN_DATA);
    ENUM_CASE(sync_pb::AutofillWalletSpecifics, PAYMENT_INSTRUMENT);
    ENUM_CASE(sync_pb::AutofillWalletSpecifics, MASKED_IBAN);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::BankAccountDetails::AccountType account_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::BankAccountDetails, AccountType,
                     ACCOUNT_TYPE_UNSPECIFIED, TRANSACTING_ACCOUNT);
  switch (account_type) {
    ENUM_CASE(sync_pb::BankAccountDetails, ACCOUNT_TYPE_UNSPECIFIED);
    ENUM_CASE(sync_pb::BankAccountDetails, CHECKING);
    ENUM_CASE(sync_pb::BankAccountDetails, SAVINGS);
    ENUM_CASE(sync_pb::BankAccountDetails, CURRENT);
    ENUM_CASE(sync_pb::BankAccountDetails, SALARY);
    ENUM_CASE(sync_pb::BankAccountDetails, TRANSACTING_ACCOUNT);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::BookmarkSpecifics::Type type) {
  ASSERT_ENUM_BOUNDS(sync_pb::BookmarkSpecifics, Type, UNSPECIFIED, FOLDER);
  switch (type) {
    ENUM_CASE(sync_pb::BookmarkSpecifics, UNSPECIFIED);
    ENUM_CASE(sync_pb::BookmarkSpecifics, URL);
    ENUM_CASE(sync_pb::BookmarkSpecifics, FOLDER);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::CommitResponse::ResponseType response_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::CommitResponse, ResponseType, SUCCESS,
                     TRANSIENT_ERROR);
  switch (response_type) {
    ENUM_CASE(sync_pb::CommitResponse, SUCCESS);
    ENUM_CASE(sync_pb::CommitResponse, CONFLICT);
    ENUM_CASE(sync_pb::CommitResponse, RETRY);
    ENUM_CASE(sync_pb::CommitResponse, INVALID_MESSAGE);
    ENUM_CASE(sync_pb::CommitResponse, OVER_QUOTA);
    ENUM_CASE(sync_pb::CommitResponse, TRANSIENT_ERROR);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::ContactInfoSpecifics::AddressType address_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::ContactInfoSpecifics, AddressType, REGULAR, WORK);
  switch (address_type) {
    ENUM_CASE(sync_pb::ContactInfoSpecifics, REGULAR);
    ENUM_CASE(sync_pb::ContactInfoSpecifics, HOME);
    ENUM_CASE(sync_pb::ContactInfoSpecifics, WORK);
  }
  NOTREACHED();
}

const char* ProtoEnumToString(
    sync_pb::ContactInfoSpecifics::VerificationStatus verification_status) {
  ASSERT_ENUM_BOUNDS(sync_pb::ContactInfoSpecifics, VerificationStatus,
                     VERIFICATION_STATUS_UNSPECIFIED, SERVER_PARSED);
  switch (verification_status) {
    ENUM_CASE(sync_pb::ContactInfoSpecifics, VERIFICATION_STATUS_UNSPECIFIED);
    ENUM_CASE(sync_pb::ContactInfoSpecifics, PARSED);
    ENUM_CASE(sync_pb::ContactInfoSpecifics, FORMATTED);
    ENUM_CASE(sync_pb::ContactInfoSpecifics, OBSERVED);
    ENUM_CASE(sync_pb::ContactInfoSpecifics, USER_VERIFIED);
    ENUM_CASE(sync_pb::ContactInfoSpecifics, SERVER_PARSED);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::TrustedVaultAutoUpgradeExperimentGroup::Type type) {
  ASSERT_ENUM_BOUNDS(sync_pb::TrustedVaultAutoUpgradeExperimentGroup, Type,
                     TYPE_UNSPECIFIED, VALIDATION);

  switch (type) {
    ENUM_CASE(sync_pb::TrustedVaultAutoUpgradeExperimentGroup,
              TYPE_UNSPECIFIED);
    ENUM_CASE(sync_pb::TrustedVaultAutoUpgradeExperimentGroup, TREATMENT);
    ENUM_CASE(sync_pb::TrustedVaultAutoUpgradeExperimentGroup, CONTROL);
    ENUM_CASE(sync_pb::TrustedVaultAutoUpgradeExperimentGroup, VALIDATION);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::NigoriSpecifics::PassphraseType type) {
  ASSERT_ENUM_BOUNDS(sync_pb::NigoriSpecifics, PassphraseType, UNKNOWN,
                     TRUSTED_VAULT_PASSPHRASE);
  switch (type) {
    ENUM_CASE(sync_pb::NigoriSpecifics, UNKNOWN);
    ENUM_CASE(sync_pb::NigoriSpecifics, IMPLICIT_PASSPHRASE);
    ENUM_CASE(sync_pb::NigoriSpecifics, KEYSTORE_PASSPHRASE);
    ENUM_CASE(sync_pb::NigoriSpecifics, FROZEN_IMPLICIT_PASSPHRASE);
    ENUM_CASE(sync_pb::NigoriSpecifics, CUSTOM_PASSPHRASE);
    ENUM_CASE(sync_pb::NigoriSpecifics, TRUSTED_VAULT_PASSPHRASE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::PaymentInstrument::SupportedRail supported_rail) {
  ASSERT_ENUM_BOUNDS(sync_pb::PaymentInstrument, SupportedRail,
                     SUPPORTED_RAIL_UNKNOWN, CARD_NUMBER);
  switch (supported_rail) {
    ENUM_CASE(sync_pb::PaymentInstrument, SUPPORTED_RAIL_UNKNOWN);
    ENUM_CASE(sync_pb::PaymentInstrument, PIX);
    ENUM_CASE(sync_pb::PaymentInstrument, IBAN);
    ENUM_CASE(sync_pb::PaymentInstrument, PAYMENT_HYPERLINK);
    ENUM_CASE(sync_pb::PaymentInstrument, CARD_NUMBER);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::PowerBookmarkSpecifics::PowerType power_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::PowerBookmarkSpecifics, PowerType,
                     POWER_TYPE_UNSPECIFIED, POWER_TYPE_NOTE);
  switch (power_type) {
    ENUM_CASE(sync_pb::PowerBookmarkSpecifics, POWER_TYPE_UNSPECIFIED);
    ENUM_CASE(sync_pb::PowerBookmarkSpecifics, POWER_TYPE_MOCK);
    ENUM_CASE(sync_pb::PowerBookmarkSpecifics, POWER_TYPE_NOTE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::NoteEntity::TargetType target_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::NoteEntity, TargetType, TARGET_TYPE_UNSPECIFIED,
                     TARGET_TYPE_PAGE);
  switch (target_type) {
    ENUM_CASE(sync_pb::NoteEntity, TARGET_TYPE_UNSPECIFIED);
    ENUM_CASE(sync_pb::NoteEntity, TARGET_TYPE_PAGE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::ReadingListSpecifics::ReadingListEntryStatus status) {
  ASSERT_ENUM_BOUNDS(sync_pb::ReadingListSpecifics, ReadingListEntryStatus,
                     UNREAD, UNSEEN);
  switch (status) {
    ENUM_CASE(sync_pb::ReadingListSpecifics, UNREAD);
    ENUM_CASE(sync_pb::ReadingListSpecifics, READ);
    ENUM_CASE(sync_pb::ReadingListSpecifics, UNSEEN);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::SavedTabGroup::SavedTabGroupColor color) {
  ASSERT_ENUM_BOUNDS(sync_pb::SavedTabGroup, SavedTabGroupColor,
                     SAVED_TAB_GROUP_COLOR_UNSPECIFIED,
                     SAVED_TAB_GROUP_COLOR_ORANGE);
  switch (color) {
    ENUM_CASE(sync_pb::SavedTabGroup, SAVED_TAB_GROUP_COLOR_UNSPECIFIED);
    ENUM_CASE(sync_pb::SavedTabGroup, SAVED_TAB_GROUP_COLOR_GREY);
    ENUM_CASE(sync_pb::SavedTabGroup, SAVED_TAB_GROUP_COLOR_BLUE);
    ENUM_CASE(sync_pb::SavedTabGroup, SAVED_TAB_GROUP_COLOR_RED);
    ENUM_CASE(sync_pb::SavedTabGroup, SAVED_TAB_GROUP_COLOR_YELLOW);
    ENUM_CASE(sync_pb::SavedTabGroup, SAVED_TAB_GROUP_COLOR_GREEN);
    ENUM_CASE(sync_pb::SavedTabGroup, SAVED_TAB_GROUP_COLOR_PINK);
    ENUM_CASE(sync_pb::SavedTabGroup, SAVED_TAB_GROUP_COLOR_PURPLE);
    ENUM_CASE(sync_pb::SavedTabGroup, SAVED_TAB_GROUP_COLOR_CYAN);
    ENUM_CASE(sync_pb::SavedTabGroup, SAVED_TAB_GROUP_COLOR_ORANGE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::SearchEngineSpecifics::ActiveStatus is_active) {
  ASSERT_ENUM_BOUNDS(sync_pb::SearchEngineSpecifics, ActiveStatus,
                     ACTIVE_STATUS_UNSPECIFIED, ACTIVE_STATUS_FALSE);
  switch (is_active) {
    ENUM_CASE(sync_pb::SearchEngineSpecifics, ACTIVE_STATUS_UNSPECIFIED);
    ENUM_CASE(sync_pb::SearchEngineSpecifics, ACTIVE_STATUS_TRUE);
    ENUM_CASE(sync_pb::SearchEngineSpecifics, ACTIVE_STATUS_FALSE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::SessionTab::FaviconType favicon_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::SessionTab, FaviconType, TYPE_WEB_FAVICON,
                     TYPE_WEB_FAVICON);
  switch (favicon_type) { ENUM_CASE(sync_pb::SessionTab, TYPE_WEB_FAVICON); }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::SharedTabGroup::Color color) {
  ASSERT_ENUM_BOUNDS(sync_pb::SharedTabGroup, Color, UNSPECIFIED, ORANGE);
  switch (color) {
    ENUM_CASE(sync_pb::SharedTabGroup, UNSPECIFIED);
    ENUM_CASE(sync_pb::SharedTabGroup, GREY);
    ENUM_CASE(sync_pb::SharedTabGroup, BLUE);
    ENUM_CASE(sync_pb::SharedTabGroup, RED);
    ENUM_CASE(sync_pb::SharedTabGroup, YELLOW);
    ENUM_CASE(sync_pb::SharedTabGroup, GREEN);
    ENUM_CASE(sync_pb::SharedTabGroup, PINK);
    ENUM_CASE(sync_pb::SharedTabGroup, PURPLE);
    ENUM_CASE(sync_pb::SharedTabGroup, CYAN);
    ENUM_CASE(sync_pb::SharedTabGroup, ORANGE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::SyncEnums::BrowserType browser_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, BrowserType, BROWSER_TYPE_UNKNOWN,
                     TYPE_AUTH_TAB);
  switch (browser_type) {
    ENUM_CASE(sync_pb::SyncEnums, BROWSER_TYPE_UNKNOWN);
    ENUM_CASE(sync_pb::SyncEnums, TYPE_TABBED);
    ENUM_CASE(sync_pb::SyncEnums, TYPE_POPUP);
    ENUM_CASE(sync_pb::SyncEnums, TYPE_CUSTOM_TAB);
    ENUM_CASE(sync_pb::SyncEnums, TYPE_AUTH_TAB);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::SyncEnums::Action action) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, Action, UPGRADE_CLIENT,
                     UNKNOWN_ACTION);
  switch (action) {
    ENUM_CASE(sync_pb::SyncEnums, UPGRADE_CLIENT);
    ENUM_CASE(sync_pb::SyncEnums, UNKNOWN_ACTION);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::SyncEnums::DeviceType device_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, DeviceType, TYPE_UNSET, TYPE_TABLET);
  switch (device_type) {
    ENUM_CASE(sync_pb::SyncEnums, TYPE_UNSET);
    ENUM_CASE(sync_pb::SyncEnums, TYPE_WIN);
    ENUM_CASE(sync_pb::SyncEnums, TYPE_MAC);
    ENUM_CASE(sync_pb::SyncEnums, TYPE_LINUX);
    ENUM_CASE(sync_pb::SyncEnums, TYPE_CROS);
    ENUM_CASE(sync_pb::SyncEnums, TYPE_OTHER);
    ENUM_CASE(sync_pb::SyncEnums, TYPE_PHONE);
    ENUM_CASE(sync_pb::SyncEnums, TYPE_TABLET);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::SyncEnums::OsType os_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, OsType, OS_TYPE_UNSPECIFIED,
                     OS_TYPE_FUCHSIA);
  switch (os_type) {
    ENUM_CASE(sync_pb::SyncEnums, OS_TYPE_UNSPECIFIED);
    ENUM_CASE(sync_pb::SyncEnums, OS_TYPE_WINDOWS);
    ENUM_CASE(sync_pb::SyncEnums, OS_TYPE_MAC);
    ENUM_CASE(sync_pb::SyncEnums, OS_TYPE_LINUX);
    ENUM_CASE(sync_pb::SyncEnums, OS_TYPE_CHROME_OS_ASH);
    ENUM_CASE(sync_pb::SyncEnums, OS_TYPE_ANDROID);
    ENUM_CASE(sync_pb::SyncEnums, OS_TYPE_IOS);
    ENUM_CASE(sync_pb::SyncEnums, OS_TYPE_CHROME_OS_LACROS);
    ENUM_CASE(sync_pb::SyncEnums, OS_TYPE_FUCHSIA);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::SyncEnums::DeviceFormFactor device_form_factor) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, DeviceFormFactor,
                     DEVICE_FORM_FACTOR_UNSPECIFIED, DEVICE_FORM_FACTOR_TV);
  switch (device_form_factor) {
    ENUM_CASE(sync_pb::SyncEnums, DEVICE_FORM_FACTOR_UNSPECIFIED);
    ENUM_CASE(sync_pb::SyncEnums, DEVICE_FORM_FACTOR_DESKTOP);
    ENUM_CASE(sync_pb::SyncEnums, DEVICE_FORM_FACTOR_PHONE);
    ENUM_CASE(sync_pb::SyncEnums, DEVICE_FORM_FACTOR_TABLET);
    ENUM_CASE(sync_pb::SyncEnums, DEVICE_FORM_FACTOR_AUTOMOTIVE);
    ENUM_CASE(sync_pb::SyncEnums, DEVICE_FORM_FACTOR_WEARABLE);
    ENUM_CASE(sync_pb::SyncEnums, DEVICE_FORM_FACTOR_TV);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::SyncEnums::ErrorType error_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, ErrorType, SUCCESS, UNKNOWN);
  switch (error_type) {
    ENUM_CASE(sync_pb::SyncEnums, SUCCESS);
    ENUM_CASE(sync_pb::SyncEnums, NOT_MY_BIRTHDAY);
    ENUM_CASE(sync_pb::SyncEnums, THROTTLED);
    ENUM_CASE(sync_pb::SyncEnums, TRANSIENT_ERROR);
    ENUM_CASE(sync_pb::SyncEnums, MIGRATION_DONE);
    ENUM_CASE(sync_pb::SyncEnums, DISABLED_BY_ADMIN);
    ENUM_CASE(sync_pb::SyncEnums, PARTIAL_FAILURE);
    ENUM_CASE(sync_pb::SyncEnums, CLIENT_DATA_OBSOLETE);
    ENUM_CASE(sync_pb::SyncEnums, ENCRYPTION_OBSOLETE);
    ENUM_CASE(sync_pb::SyncEnums, UNKNOWN);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::SyncEnums::GetUpdatesOrigin origin) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, GetUpdatesOrigin, UNKNOWN_ORIGIN,
                     PROGRAMMATIC);
  switch (origin) {
    ENUM_CASE(sync_pb::SyncEnums, UNKNOWN_ORIGIN);
    ENUM_CASE(sync_pb::SyncEnums, PERIODIC);
    ENUM_CASE(sync_pb::SyncEnums, NEWLY_SUPPORTED_DATATYPE);
    ENUM_CASE(sync_pb::SyncEnums, MIGRATION);
    ENUM_CASE(sync_pb::SyncEnums, NEW_CLIENT);
    ENUM_CASE(sync_pb::SyncEnums, RECONFIGURATION);
    ENUM_CASE(sync_pb::SyncEnums, GU_TRIGGER);
    ENUM_CASE(sync_pb::SyncEnums, RETRY);
    ENUM_CASE(sync_pb::SyncEnums, PROGRAMMATIC);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::SyncEnums::PageTransition page_transition) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, PageTransition, LINK,
                     KEYWORD_GENERATED);
  switch (page_transition) {
    ENUM_CASE(sync_pb::SyncEnums, LINK);
    ENUM_CASE(sync_pb::SyncEnums, TYPED);
    ENUM_CASE(sync_pb::SyncEnums, AUTO_BOOKMARK);
    ENUM_CASE(sync_pb::SyncEnums, AUTO_SUBFRAME);
    ENUM_CASE(sync_pb::SyncEnums, MANUAL_SUBFRAME);
    ENUM_CASE(sync_pb::SyncEnums, GENERATED);
    ENUM_CASE(sync_pb::SyncEnums, AUTO_TOPLEVEL);
    ENUM_CASE(sync_pb::SyncEnums, FORM_SUBMIT);
    ENUM_CASE(sync_pb::SyncEnums, RELOAD);
    ENUM_CASE(sync_pb::SyncEnums, KEYWORD);
    ENUM_CASE(sync_pb::SyncEnums, KEYWORD_GENERATED);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::SyncEnums::PageTransitionRedirectType page_transition_qualifier) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, PageTransitionRedirectType,
                     CLIENT_REDIRECT, SERVER_REDIRECT);
  switch (page_transition_qualifier) {
    ENUM_CASE(sync_pb::SyncEnums, CLIENT_REDIRECT);
    ENUM_CASE(sync_pb::SyncEnums, SERVER_REDIRECT);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::SyncEnums::SendTabReceivingType send_tab_receiving_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, SendTabReceivingType,
                     SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
                     SEND_TAB_RECEIVING_TYPE_CHROME_AND_PUSH_NOTIFICATION);
  switch (send_tab_receiving_type) {
    ENUM_CASE(sync_pb::SyncEnums,
              SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED);
    ENUM_CASE(sync_pb::SyncEnums,
              SEND_TAB_RECEIVING_TYPE_CHROME_AND_PUSH_NOTIFICATION);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::SyncEnums::SingletonDebugEventType type) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, SingletonDebugEventType,
                     CONNECTION_STATUS_CHANGE, TRUSTED_VAULT_KEY_ACCEPTED);
  switch (type) {
    ENUM_CASE(sync_pb::SyncEnums, CONNECTION_STATUS_CHANGE);
    ENUM_CASE(sync_pb::SyncEnums, UPDATED_TOKEN);
    ENUM_CASE(sync_pb::SyncEnums, PASSPHRASE_REQUIRED);
    ENUM_CASE(sync_pb::SyncEnums, PASSPHRASE_ACCEPTED);
    ENUM_CASE(sync_pb::SyncEnums, INITIALIZATION_COMPLETE);
    ENUM_CASE(sync_pb::SyncEnums, STOP_SYNCING_PERMANENTLY);
    ENUM_CASE(sync_pb::SyncEnums, ACTIONABLE_ERROR);
    ENUM_CASE(sync_pb::SyncEnums, ENCRYPTED_TYPES_CHANGED);
    ENUM_CASE(sync_pb::SyncEnums, PASSPHRASE_TYPE_CHANGED);
    ENUM_CASE(sync_pb::SyncEnums, DEPRECATED_KEYSTORE_TOKEN_UPDATED);
    ENUM_CASE(sync_pb::SyncEnums, CONFIGURE_COMPLETE);
    ENUM_CASE(sync_pb::SyncEnums, DEPRECATED_BOOTSTRAP_TOKEN_UPDATED);
    ENUM_CASE(sync_pb::SyncEnums, TRUSTED_VAULT_KEY_REQUIRED);
    ENUM_CASE(sync_pb::SyncEnums, TRUSTED_VAULT_KEY_ACCEPTED);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::TabNavigation::BlockedState state) {
  ASSERT_ENUM_BOUNDS(sync_pb::TabNavigation, BlockedState, STATE_ALLOWED,
                     STATE_BLOCKED);
  switch (state) {
    ENUM_CASE(sync_pb::TabNavigation, STATE_ALLOWED);
    ENUM_CASE(sync_pb::TabNavigation, STATE_BLOCKED);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::SyncEnums::PasswordState state) {
  ASSERT_ENUM_BOUNDS(sync_pb::SyncEnums, PasswordState, PASSWORD_STATE_UNKNOWN,
                     HAS_PASSWORD_FIELD);
  switch (state) {
    ENUM_CASE(sync_pb::SyncEnums, PASSWORD_STATE_UNKNOWN);
    ENUM_CASE(sync_pb::SyncEnums, NO_PASSWORD_FIELD);
    ENUM_CASE(sync_pb::SyncEnums, HAS_PASSWORD_FIELD);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::UserConsentTypes::ConsentStatus status) {
  ASSERT_ENUM_BOUNDS(sync_pb::UserConsentTypes, ConsentStatus,
                     CONSENT_STATUS_UNSPECIFIED, GIVEN);
  switch (status) {
    ENUM_CASE(sync_pb::UserConsentTypes, CONSENT_STATUS_UNSPECIFIED);
    ENUM_CASE(sync_pb::UserConsentTypes, NOT_GIVEN);
    ENUM_CASE(sync_pb::UserConsentTypes, GIVEN);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::GaiaPasswordReuse::PasswordReuseDetected::SafeBrowsingStatus::
        ReportingPopulation safe_browsing_reporting_population) {
  ASSERT_ENUM_BOUNDS(
      sync_pb::GaiaPasswordReuse::PasswordReuseDetected::SafeBrowsingStatus,
      ReportingPopulation, REPORTING_POPULATION_UNSPECIFIED,
      ENHANCED_PROTECTION);
  switch (safe_browsing_reporting_population) {
    ENUM_CASE(
        sync_pb::GaiaPasswordReuse::PasswordReuseDetected::SafeBrowsingStatus,
        REPORTING_POPULATION_UNSPECIFIED);
    ENUM_CASE(
        sync_pb::GaiaPasswordReuse::PasswordReuseDetected::SafeBrowsingStatus,
        NONE);
    ENUM_CASE(
        sync_pb::GaiaPasswordReuse::PasswordReuseDetected::SafeBrowsingStatus,
        EXTENDED_REPORTING);
    ENUM_CASE(
        sync_pb::GaiaPasswordReuse::PasswordReuseDetected::SafeBrowsingStatus,
        SCOUT);
    ENUM_CASE(
        sync_pb::GaiaPasswordReuse::PasswordReuseDetected::SafeBrowsingStatus,
        ENHANCED_PROTECTION);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction::
        InteractionResult interaction_result) {
  ASSERT_ENUM_BOUNDS(sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction,
                     InteractionResult, UNSPECIFIED,
                     WARNING_ACTION_TAKEN_ON_SETTINGS);
  switch (interaction_result) {
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction,
              UNSPECIFIED);
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction,
              WARNING_ACTION_TAKEN);
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction,
              WARNING_ACTION_IGNORED);
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction,
              WARNING_UI_IGNORED);
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction,
              WARNING_ACTION_TAKEN_ON_SETTINGS);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::GaiaPasswordReuse::PasswordReuseLookup::LookupResult
        lookup_result) {
  ASSERT_ENUM_BOUNDS(sync_pb::GaiaPasswordReuse::PasswordReuseLookup,
                     LookupResult, UNSPECIFIED, TURNED_OFF_BY_POLICY);
  switch (lookup_result) {
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseLookup, UNSPECIFIED);
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseLookup, ALLOWLIST_HIT);
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseLookup, CACHE_HIT);
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseLookup, REQUEST_SUCCESS);
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseLookup, REQUEST_FAILURE);
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseLookup, URL_UNSUPPORTED);
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseLookup,
              ENTERPRISE_ALLOWLIST_HIT);
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseLookup,
              TURNED_OFF_BY_POLICY);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::GaiaPasswordReuse::PasswordReuseLookup::ReputationVerdict
        verdict) {
  ASSERT_ENUM_BOUNDS(sync_pb::GaiaPasswordReuse::PasswordReuseLookup,
                     ReputationVerdict, VERDICT_UNSPECIFIED, PHISHING);
  switch (verdict) {
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseLookup,
              VERDICT_UNSPECIFIED);
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseLookup, SAFE);
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseLookup, LOW_REPUTATION);
    ENUM_CASE(sync_pb::GaiaPasswordReuse::PasswordReuseLookup, PHISHING);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::UserEventSpecifics::GaiaPasswordCaptured::EventTrigger trigger) {
  ASSERT_ENUM_BOUNDS(sync_pb::UserEventSpecifics::GaiaPasswordCaptured,
                     EventTrigger, UNSPECIFIED, EXPIRED_28D_TIMER);
  switch (trigger) {
    ENUM_CASE(sync_pb::UserEventSpecifics::GaiaPasswordCaptured, UNSPECIFIED);
    ENUM_CASE(sync_pb::UserEventSpecifics::GaiaPasswordCaptured,
              USER_LOGGED_IN);
    ENUM_CASE(sync_pb::UserEventSpecifics::GaiaPasswordCaptured,
              EXPIRED_28D_TIMER);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::UserEventSpecifics::FlocIdComputed::EventTrigger trigger) {
  ASSERT_ENUM_BOUNDS(sync_pb::UserEventSpecifics::FlocIdComputed, EventTrigger,
                     UNSPECIFIED, HISTORY_DELETE);
  switch (trigger) {
    ENUM_CASE(sync_pb::UserEventSpecifics::FlocIdComputed, UNSPECIFIED);
    ENUM_CASE(sync_pb::UserEventSpecifics::FlocIdComputed, NEW);
    ENUM_CASE(sync_pb::UserEventSpecifics::FlocIdComputed, REFRESHED);
    ENUM_CASE(sync_pb::UserEventSpecifics::FlocIdComputed, HISTORY_DELETE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WalletMaskedCreditCard::CardInfoRetrievalEnrollmentState
        card_info_retrieval_enrollment_state) {
  ASSERT_ENUM_BOUNDS(sync_pb::WalletMaskedCreditCard,
                     CardInfoRetrievalEnrollmentState, RETRIEVAL_UNSPECIFIED,
                     RETRIEVAL_UNENROLLED_AND_ELIGIBLE);
  switch (card_info_retrieval_enrollment_state) {
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, RETRIEVAL_UNSPECIFIED);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, RETRIEVAL_ENROLLED);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard,
              RETRIEVAL_UNENROLLED_AND_NOT_ELIGIBLE);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard,
              RETRIEVAL_UNENROLLED_AND_ELIGIBLE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WalletMaskedCreditCard::VirtualCardEnrollmentState
        virtual_card_enrollment_state) {
  ASSERT_ENUM_BOUNDS(sync_pb::WalletMaskedCreditCard,
                     VirtualCardEnrollmentState, UNSPECIFIED,
                     UNENROLLED_AND_ELIGIBLE);
  switch (virtual_card_enrollment_state) {
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, UNSPECIFIED);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, UNENROLLED);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, ENROLLED);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, UNENROLLED_AND_NOT_ELIGIBLE);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, UNENROLLED_AND_ELIGIBLE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WalletMaskedCreditCard::VirtualCardEnrollmentType
        virtual_card_enrollment_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::WalletMaskedCreditCard, VirtualCardEnrollmentType,
                     TYPE_UNSPECIFIED, NETWORK);
  switch (virtual_card_enrollment_type) {
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, TYPE_UNSPECIFIED);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, ISSUER);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, NETWORK);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WalletMaskedCreditCard::WalletCardStatus wallet_card_status) {
  ASSERT_ENUM_BOUNDS(sync_pb::WalletMaskedCreditCard, WalletCardStatus, VALID,
                     EXPIRED);
  switch (wallet_card_status) {
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, VALID);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, EXPIRED);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WalletMaskedCreditCard::WalletCardType wallet_card_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::WalletMaskedCreditCard, WalletCardType, UNKNOWN,
                     VERVE);
  switch (wallet_card_type) {
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, UNKNOWN);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, AMEX);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, DISCOVER);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, JCB);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, MAESTRO);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, MASTER_CARD);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, SOLO);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, SWITCH);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, VISA);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, UNIONPAY);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, ELO);
    ENUM_CASE(sync_pb::WalletMaskedCreditCard, VERVE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::CardBenefit::CategoryBenefitType category_benefit_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::CardBenefit, CategoryBenefitType,
                     CATEGORY_BENEFIT_TYPE_UNKNOWN, GROCERY_STORES);
  switch (category_benefit_type) {
    ENUM_CASE(sync_pb::CardBenefit, CATEGORY_BENEFIT_TYPE_UNKNOWN);
    ENUM_CASE(sync_pb::CardBenefit, SUBSCRIPTION);
    ENUM_CASE(sync_pb::CardBenefit, FLIGHTS);
    ENUM_CASE(sync_pb::CardBenefit, DINING);
    ENUM_CASE(sync_pb::CardBenefit, ENTERTAINMENT);
    ENUM_CASE(sync_pb::CardBenefit, STREAMING);
    ENUM_CASE(sync_pb::CardBenefit, GROCERY_STORES);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::CardIssuer::Issuer issuer) {
  ASSERT_ENUM_BOUNDS(sync_pb::CardIssuer, Issuer, ISSUER_UNKNOWN,
                     EXTERNAL_ISSUER);
  switch (issuer) {
    ENUM_CASE(sync_pb::CardIssuer, ISSUER_UNKNOWN);
    ENUM_CASE(sync_pb::CardIssuer, GOOGLE);
    ENUM_CASE(sync_pb::CardIssuer, EXTERNAL_ISSUER);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WalletMetadataSpecifics::Type wallet_metadata_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::WalletMetadataSpecifics, Type, UNKNOWN, IBAN);
  switch (wallet_metadata_type) {
    ENUM_CASE(sync_pb::WalletMetadataSpecifics, UNKNOWN);
    ENUM_CASE(sync_pb::WalletMetadataSpecifics, CARD);
    ENUM_CASE(sync_pb::WalletMetadataSpecifics, ADDRESS);
    ENUM_CASE(sync_pb::WalletMetadataSpecifics, IBAN);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::WebApkIconInfo::Purpose purpose) {
  ASSERT_ENUM_BOUNDS(sync_pb::WebApkIconInfo, Purpose, UNSPECIFIED, MONOCHROME);
  switch (purpose) {
    ENUM_CASE(sync_pb::WebApkIconInfo, UNSPECIFIED);
    ENUM_CASE(sync_pb::WebApkIconInfo, ANY);
    ENUM_CASE(sync_pb::WebApkIconInfo, MASKABLE);
    ENUM_CASE(sync_pb::WebApkIconInfo, MONOCHROME);
  }
  NOTREACHED();
}

const char* ProtoEnumToString(sync_pb::WebAppIconInfo::Purpose purpose) {
  ASSERT_ENUM_BOUNDS(sync_pb::WebAppIconInfo, Purpose, UNSPECIFIED, MONOCHROME);
  switch (purpose) {
    ENUM_CASE(sync_pb::WebAppIconInfo, UNSPECIFIED);
    ENUM_CASE(sync_pb::WebAppIconInfo, ANY);
    ENUM_CASE(sync_pb::WebAppIconInfo, MASKABLE);
    ENUM_CASE(sync_pb::WebAppIconInfo, MONOCHROME);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WebAppSpecifics::UserDisplayMode user_display_mode) {
  ASSERT_ENUM_BOUNDS(sync_pb::WebAppSpecifics, UserDisplayMode, UNSPECIFIED,
                     TABBED);
  switch (user_display_mode) {
    ENUM_CASE(sync_pb::WebAppSpecifics, UNSPECIFIED);
    ENUM_CASE(sync_pb::WebAppSpecifics, BROWSER);
    ENUM_CASE(sync_pb::WebAppSpecifics, STANDALONE);
    ENUM_CASE(sync_pb::WebAppSpecifics, TABBED);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WifiConfigurationSpecifics::SecurityType security_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::WifiConfigurationSpecifics, SecurityType,
                     SECURITY_TYPE_UNSPECIFIED, SECURITY_TYPE_PSK);
  switch (security_type) {
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics, SECURITY_TYPE_UNSPECIFIED);
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics, SECURITY_TYPE_NONE);
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics, SECURITY_TYPE_WEP);
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics, SECURITY_TYPE_PSK);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WifiConfigurationSpecifics::AutomaticallyConnectOption
        automatically_connect_option) {
  ASSERT_ENUM_BOUNDS(
      sync_pb::WifiConfigurationSpecifics, AutomaticallyConnectOption,
      AUTOMATICALLY_CONNECT_UNSPECIFIED, AUTOMATICALLY_CONNECT_ENABLED);
  switch (automatically_connect_option) {
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics,
              AUTOMATICALLY_CONNECT_UNSPECIFIED);
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics,
              AUTOMATICALLY_CONNECT_DISABLED);
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics,
              AUTOMATICALLY_CONNECT_ENABLED);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WifiConfigurationSpecifics::IsPreferredOption
        is_preferred_option) {
  ASSERT_ENUM_BOUNDS(sync_pb::WifiConfigurationSpecifics, IsPreferredOption,
                     IS_PREFERRED_UNSPECIFIED, IS_PREFERRED_ENABLED);
  switch (is_preferred_option) {
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics, IS_PREFERRED_UNSPECIFIED);
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics, IS_PREFERRED_DISABLED);
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics, IS_PREFERRED_ENABLED);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WifiConfigurationSpecifics::MeteredOption metered_option) {
  ASSERT_ENUM_BOUNDS(sync_pb::WifiConfigurationSpecifics, MeteredOption,
                     METERED_OPTION_UNSPECIFIED, METERED_OPTION_AUTO);
  switch (metered_option) {
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics, METERED_OPTION_UNSPECIFIED);
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics, METERED_OPTION_NO);
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics, METERED_OPTION_YES);
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics, METERED_OPTION_AUTO);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WifiConfigurationSpecifics::ProxyConfiguration::ProxyOption
        proxy_option) {
  ASSERT_ENUM_BOUNDS(sync_pb::WifiConfigurationSpecifics::ProxyConfiguration,
                     ProxyOption, PROXY_OPTION_UNSPECIFIED,
                     PROXY_OPTION_MANUAL);
  switch (proxy_option) {
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics::ProxyConfiguration,
              PROXY_OPTION_UNSPECIFIED);
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics::ProxyConfiguration,
              PROXY_OPTION_DISABLED);
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics::ProxyConfiguration,
              PROXY_OPTION_AUTOMATIC);
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics::ProxyConfiguration,
              PROXY_OPTION_AUTODISCOVERY);
    ENUM_CASE(sync_pb::WifiConfigurationSpecifics::ProxyConfiguration,
              PROXY_OPTION_MANUAL);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WorkspaceDeskSpecifics::WindowState window_state) {
  ASSERT_ENUM_BOUNDS(sync_pb::WorkspaceDeskSpecifics, WindowState,
                     UNKNOWN_WINDOW_STATE, FLOATED);
  switch (window_state) {
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, UNKNOWN_WINDOW_STATE);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, NORMAL);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, MINIMIZED);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, MAXIMIZED);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, FULLSCREEN);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, PRIMARY_SNAPPED);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, SECONDARY_SNAPPED);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, FLOATED);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WorkspaceDeskSpecifics::LaunchContainer container) {
  ASSERT_ENUM_BOUNDS(sync_pb::WorkspaceDeskSpecifics, LaunchContainer,
                     LAUNCH_CONTAINER_UNSPECIFIED, LAUNCH_CONTAINER_NONE);
  switch (container) {
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, LAUNCH_CONTAINER_UNSPECIFIED);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, LAUNCH_CONTAINER_WINDOW);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics,
              LAUNCH_CONTAINER_PANEL_DEPRECATED);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, LAUNCH_CONTAINER_TAB);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, LAUNCH_CONTAINER_NONE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WorkspaceDeskSpecifics::WindowOpenDisposition disposition) {
  ASSERT_ENUM_BOUNDS(sync_pb::WorkspaceDeskSpecifics, WindowOpenDisposition,
                     UNKNOWN, NEW_PICTURE_IN_PICTURE);
  switch (disposition) {
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, UNKNOWN);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, CURRENT_TAB);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, SINGLETON_TAB);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, NEW_FOREGROUND_TAB);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, NEW_BACKGROUND_TAB);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, NEW_POPUP);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, NEW_WINDOW);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, SAVE_TO_DISK);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, OFF_THE_RECORD);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, IGNORE_ACTION);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, SWITCH_TO_TAB);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, NEW_PICTURE_IN_PICTURE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::UserConsentTypes::AssistantActivityControlConsent::SettingType
        setting_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::UserConsentTypes::AssistantActivityControlConsent,
                     SettingType, SETTING_TYPE_UNSPECIFIED, DEVICE_APPS);
  switch (setting_type) {
    ENUM_CASE(sync_pb::UserConsentTypes::AssistantActivityControlConsent,
              SETTING_TYPE_UNSPECIFIED);
    ENUM_CASE(sync_pb::UserConsentTypes::AssistantActivityControlConsent, ALL);
    ENUM_CASE(sync_pb::UserConsentTypes::AssistantActivityControlConsent,
              WEB_AND_APP_ACTIVITY);
    ENUM_CASE(sync_pb::UserConsentTypes::AssistantActivityControlConsent,
              DEVICE_APPS);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::WorkspaceDeskSpecifics::DeskType type) {
  ASSERT_ENUM_BOUNDS(sync_pb::WorkspaceDeskSpecifics, DeskType, UNKNOWN_TYPE,
                     FLOATING_WORKSPACE);
  switch (type) {
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, UNKNOWN_TYPE);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, TEMPLATE);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, SAVE_AND_RECALL);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, FLOATING_WORKSPACE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::WorkspaceDeskSpecifics::TabGroupColor color) {
  ASSERT_ENUM_BOUNDS(sync_pb::WorkspaceDeskSpecifics, TabGroupColor,
                     UNKNOWN_COLOR, ORANGE);

  switch (color) {
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, UNKNOWN_COLOR);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, GREY);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, BLUE);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, RED);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, YELLOW);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, GREEN);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, PINK);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, PURPLE);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, CYAN);
    ENUM_CASE(sync_pb::WorkspaceDeskSpecifics, ORANGE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(sync_pb::DataTypeState::InitialSyncState state) {
  ASSERT_ENUM_BOUNDS(sync_pb::DataTypeState, InitialSyncState,
                     INITIAL_SYNC_STATE_UNSPECIFIED, INITIAL_SYNC_UNNECESSARY);
  switch (state) {
    ENUM_CASE(sync_pb::DataTypeState, INITIAL_SYNC_STATE_UNSPECIFIED);
    ENUM_CASE(sync_pb::DataTypeState, INITIAL_SYNC_PARTIALLY_DONE);
    ENUM_CASE(sync_pb::DataTypeState, INITIAL_SYNC_DONE);
    ENUM_CASE(sync_pb::DataTypeState, INITIAL_SYNC_UNNECESSARY);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::CookieSpecifics::CookieSameSite site_restrictions) {
  ASSERT_ENUM_BOUNDS(sync_pb::CookieSpecifics, CookieSameSite, UNSPECIFIED,
                     STRICT_MODE);
  switch (site_restrictions) {
    ENUM_CASE(sync_pb::CookieSpecifics, UNSPECIFIED);
    ENUM_CASE(sync_pb::CookieSpecifics, NO_RESTRICTION);
    ENUM_CASE(sync_pb::CookieSpecifics, LAX_MODE);
    ENUM_CASE(sync_pb::CookieSpecifics, STRICT_MODE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::CookieSpecifics::CookiePriority priority) {
  ASSERT_ENUM_BOUNDS(sync_pb::CookieSpecifics, CookiePriority,
                     UNSPECIFIED_PRIORITY, HIGH);
  switch (priority) {
    ENUM_CASE(sync_pb::CookieSpecifics, UNSPECIFIED_PRIORITY);
    ENUM_CASE(sync_pb::CookieSpecifics, LOW);
    ENUM_CASE(sync_pb::CookieSpecifics, MEDIUM);
    ENUM_CASE(sync_pb::CookieSpecifics, HIGH);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::CookieSpecifics::CookieSourceScheme source_scheme) {
  ASSERT_ENUM_BOUNDS(sync_pb::CookieSpecifics, CookieSourceScheme, UNSET,
                     SECURE);
  switch (source_scheme) {
    ENUM_CASE(sync_pb::CookieSpecifics, UNSET);
    ENUM_CASE(sync_pb::CookieSpecifics, NON_SECURE);
    ENUM_CASE(sync_pb::CookieSpecifics, SECURE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::CookieSpecifics::CookieSourceType source_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::CookieSpecifics, CookieSourceType, UNKNOWN,
                     OTHER);
  switch (source_type) {
    ENUM_CASE(sync_pb::CookieSpecifics, UNKNOWN);
    ENUM_CASE(sync_pb::CookieSpecifics, HTTP);
    ENUM_CASE(sync_pb::CookieSpecifics, SCRIPT);
    ENUM_CASE(sync_pb::CookieSpecifics, OTHER);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::SharingMessageSpecifics::ChannelConfiguration::
        ChimeChannelConfiguration::ChimeChannelType channel_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::SharingMessageSpecifics::ChannelConfiguration::
                         ChimeChannelConfiguration,
                     ChimeChannelType, CHANNEL_TYPE_UNSPECIFIED, APPLE_PUSH);
  switch (channel_type) {
    ENUM_CASE(sync_pb::SharingMessageSpecifics::ChannelConfiguration::
                  ChimeChannelConfiguration,
              CHANNEL_TYPE_UNSPECIFIED);
    ENUM_CASE(sync_pb::SharingMessageSpecifics::ChannelConfiguration::
                  ChimeChannelConfiguration,
              APPLE_PUSH);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::ThemeSpecifics::UserColorTheme::BrowserColorVariant
        browser_color_variant) {
  ASSERT_ENUM_BOUNDS(sync_pb::ThemeSpecifics::UserColorTheme,
                     BrowserColorVariant, BROWSER_COLOR_VARIANT_UNSPECIFIED,
                     EXPRESSIVE);
  switch (browser_color_variant) {
    ENUM_CASE(sync_pb::ThemeSpecifics::UserColorTheme,
              BROWSER_COLOR_VARIANT_UNSPECIFIED);
    ENUM_CASE(sync_pb::ThemeSpecifics::UserColorTheme, SYSTEM);
    ENUM_CASE(sync_pb::ThemeSpecifics::UserColorTheme, TONAL_SPOT);
    ENUM_CASE(sync_pb::ThemeSpecifics::UserColorTheme, NEUTRAL);
    ENUM_CASE(sync_pb::ThemeSpecifics::UserColorTheme, VIBRANT);
    ENUM_CASE(sync_pb::ThemeSpecifics::UserColorTheme, EXPRESSIVE);
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::ThemeSpecifics::BrowserColorScheme browser_color_scheme) {
  ASSERT_ENUM_BOUNDS(sync_pb::ThemeSpecifics, BrowserColorScheme,
                     BROWSER_COLOR_SCHEME_UNSPECIFIED, DARK);
  switch (browser_color_scheme) {
    ENUM_CASE(sync_pb::ThemeSpecifics, BROWSER_COLOR_SCHEME_UNSPECIFIED);
    ENUM_CASE(sync_pb::ThemeSpecifics, SYSTEM);
    ENUM_CASE(sync_pb::ThemeSpecifics, LIGHT);
    ENUM_CASE(sync_pb::ThemeSpecifics, DARK);
  }
}

#undef ASSERT_ENUM_BOUNDS
#undef ENUM_CASE

}  // namespace syncer
