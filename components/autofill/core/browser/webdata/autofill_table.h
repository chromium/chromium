// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/sync_metadata_store.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

namespace base {
class Time;
}

namespace syncer {
class MetadataBatch;
}

namespace autofill {

class AutocompleteChange;
class AutocompleteEntry;
struct AutofillMetadata;
class AutofillOfferData;
class AutofillTableEncryptor;
class AutofillTableTest;
class BankAccount;
class CreditCard;
struct CreditCardCloudTokenData;
struct FormFieldData;
class Iban;
struct PaymentsCustomerData;
class VirtualCardUsageData;
// Helper struct to better group server cvc related variables for better
// passing last_updated_timestamp, which is needed for sync bridge. Limited
// scope in autofill table & sync bridge.
struct ServerCvc {
  bool operator==(const ServerCvc&) const = default;
  // A server generated id to identify the corresponding credit card.
  const int64_t instrument_id;
  // CVC value of the card.
  const std::u16string cvc;
  // The timestamp of the most recent update to the data entry.
  const base::Time last_updated_timestamp;
};

// This class manages the various Autofill tables within the SQLite database
// passed to the constructor. It expects the following schemas:
//
// Note: The database stores time in seconds, UTC.
//
// autofill             This table contains autocomplete history data (not
//                      structured information).
//
//   name               The name of the input as specified in the html.
//   value              The literal contents of the text field.
//   value_lower        The contents of the text field made lower_case.
//   date_created       The date on which the user first entered the string
//                      |value| into a field of name |name|.
//   date_last_used     The date on which the user last entered the string
//                      |value| into a field of name |name|.
//   count              How many times the user has entered the string |value|
//                      in a field of name |name|.
//
// DEPRECATED. Use local_addresses instead.
// autofill_profiles    This table contains Autofill profile data added by the
//                      user with the Autofill dialog.  Most of the columns are
//                      standard entries in a contact information form.
//
//   guid               A guid string to uniquely identify the profile.
//                      Added in version 31.
//   label              A user-chosen and user-visible label for the profile to
//                      help identifying the semantics of the profile. The user
//                      can choose an arbitrary string in principle, but the
//                      values '$HOME$' and '$WORK$' indicate a special meaning.
//   company_name
//   street_address     The combined lines of the street address.
//                      Added in version 54.
//   dependent_locality
//                      A sub-classification beneath the city, e.g. an
//                      inner-city district or suburb.  Added in version 54.
//   city
//   state
//   zipcode
//   sorting_code       Similar to the zipcode column, but used for businesses
//                      or organizations that might not be geographically
//                      contiguous.  The canonical example is CEDEX in France.
//                      Added in version 54.
//   country_code
//   use_count          The number of times this profile has been used to fill
//                      a form. Added in version 61.
//   use_date           The date this profile was last used to fill a form,
//                      in time_t. Added in version 61.
//   date_modified      The date on which this profile was last modified, in
//                      time_t. Added in version 30.
//   origin             The domain of origin for this profile.
//                      Added in version 50.
//   language_code      The BCP 47 language code used to format the address for
//                      display. For example, a JP address with "ja" language
//                      code starts with the postal code, but a JP address with
//                      "ja-latn" language code starts with the recipient name.
//                      Added in version 56.
//   disallow_settings_visible_updates
//                      If true, a profile does not qualify to get merged with
//                      a profile observed in a form submission.
//
// DEPRECATED. See autofill_profiles.
// autofill_profile_addresses
//   guid               The guid string that identifies the profile to which
//                      the name belongs.
//                      This table stores the structured address information.
//   street_address     Stores the street address. This field is also stored in
//                      the profile table and is used to detect if a legacy
//                      client that does not support writing to this table
//                      changed the address. If this is true, the address stored
//                      in the table is removed.
//   street_name        The name of the street.
//   dependent_street_name
//                      The name of the crossing street.
//   house_number       The house number.
//   subpremise         The floor, apartment number and staircase.
//                      apartment number.
//   dependent_locality
//                      A sub-classification beneath the city, e.g. an
//                      inner-city district or suburb.
//   city               The city information of the address.
//   state              The state information of the address.
//   zip_code           The zip code of the address.
//   country_code       The code of the country of the address.
//   sorting_code       Similar to the zipcode column, but used for businesses
//                      or organizations that might not be geographically
//                      contiguous.
//   premise_name       The name of the premise.
//   apartment_number   The number of the apartment.
//   floor              The floor in which the apartment is located.
//   street_address_status
//   street_name_status
//   dependent_street_name_status
//   house_number_status
//   subpremise_status
//   premise_name_status
//   dependent_locality_status
//   city_status
//   state_status
//   zip_code_status
//   country_code_status
//   sorting_code_status
//   apartment_number_status
//   floor_status
//                      Each token of the address has an additional validation
//                      status that indicates if Autofill parsed the value out
//                      of an unstructured (last) name, or if autofill formatted
//                      the token from its structured subcomponents, or if the
//                      value was observed in a form submission, or even
//                      validated by the user in the settings.
//
// DEPRECATED. See autofill_profiles.
// autofill_profile_names
//                      This table contains the multi-valued name fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which
//                      the name belongs.
//   honorific_prefix   The honorific prefix of a person like Ms, Mr or Prof
//   first_name         The first name of a person.
//   middle_name        The middle name or even names of a person.
//   last_name          The unstructured last name that is a combination of the
//                      first and second last name.
//   first_last_name    The first part of the last name. Mostly used for
//                      Latinx/Hispanic last names.
//   conjunction_last_name
//                      An optional conjunction that is mostly used in
//                      Hispanic/Latinx last names in between the first and
//                      second last name in the unstructured representation.
//   second_last_name   The second part of the last names. Last names only
//                      consisting of a single part are stored in the second
//                      part by default.
//   full_name          The unstructured full name of a person.
//   full_name_with_honorific_prefix
//                      The combination of the full name and the honorific
//                      prefix.
//   honorific_prefix_status
//   first_name_status
//   middle_name_status
//   last_name_status
//   first_last_name_status
//   conjunction_last_name_status
//   second_last_name_status
//   full_name_status
//   full_name_with_honorific_prefix_status
//                      Each token of the names has an additional validation
//                      status that indicates if Autofill parsed the value out
//                      of an unstructured (last) name, or if autofill formatted
//                      the token from its structured subcomponents, or if the
//                      value was observed in a form submission, or even
//                      validated by the user in the settings.
//
// DEPRECATED. See autofill_profiles.
// autofill_profile_emails
//                      This table contains the multi-valued email fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which
//                      the email belongs.
//   email
//
// DEPRECATED. See autofill_profiles.
// autofill_profile_phones
//                      This table contains the multi-valued phone fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which the
//                      phone number belongs.
//   number
//
// DEPRECATED. See autofill_profiles.
// autofill_profile_birthdates
//                      This table contains the multi-valued birthdate fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which the
//                      birthdate number belongs.
//   day                As an integer between 1 and 31 inclusive, or 0 if unset.
//   month              As an integer between 1 and 12 inclusive, or 0 if unset.
//   year               As a 4 digit integer, or 0 if unset.
//
// credit_cards         This table contains credit card data added by the user
//                      with the Autofill dialog.  Most of the columns are
//                      standard entries in a credit card form.
//
//   guid               A guid string to uniquely identify the credit card.
//                      Added in version 31.
//   name_on_card
//   expiration_month
//   expiration_year
//   card_number_encrypted
//                      Stores encrypted credit card number.
//   use_count          The number of times this card has been used to fill
//                      a form. Added in version 61.
//   use_date           The date this card was last used to fill a form,
//                      in time_t. Added in version 61.
//   date_modified      The date on which this entry was last modified, in
//                      time_t. Added in version 30.
//   origin             The domain of origin for this profile.
//                      Added in version 50.
//   billing_address_id The guid string that identifies the local profile which
//                      is the billing address for this card. Can be null in the
//                      database, but always returned as an empty string in
//                      CreditCard. Added in version 66.
//   nickname           A nickname for the card, entered by the user. Added in
//                      version 87.
//
// masked_credit_cards
//                      This table contains "masked" credit card information
//                      about credit cards stored on the server. It consists
//                      of a short description and an ID, but not full payment
//                      information. Writing to this table is done by sync and
//                      on successful save of card to the server.
//                      When a server card is unmasked, it will stay here and
//                      will additionally be added in unmasked_credit_cards.
//
//   id                 String assigned by the server to identify this card.
//                      This is a legacy version of instrument_id and is opaque
//                      to the client.
//   status             Server's status of this card.
//                      TODO(brettw) define constants for this.
//   name_on_card
//   network            Issuer network of the card. For example, "VISA". Renamed
//                      from "type" in version 72.
//   last_four          Last four digits of the card number. For de-duping
//                      with locally stored cards and generating descriptions.
//   exp_month          Expiration month: 1-12
//   exp_year           Four-digit year: 2017
//   bank_name          Issuer bank name of the credit card.
//   nickname           The card's nickname, if it exists. Added in version 84.
//   card_issuer        Issuer for the card. An integer representing the
//                      CardIssuer.Issuer enum from the Chrome Sync response.
//                      For example, GOOGLE or ISSUER_UNKNOWN.
//   instrument_id      Credit card id assigned by the server to identify this
//                      card. This is opaque to the client, and |id| is the
//                      legacy version of this.
//   virtual_card_enrollment_state
//                      An enum indicating the virtual card enrollment state of
//                      this card. kUnspecified is the default value.
//                      kUnenrolled means this card has not been enrolled to
//                      have virtual cards. kEnrolled means the card has been
//                      enrolled and has related virtual credit cards.
//   card_art_url       URL to generate the card art image for this card.
//   product_description
//                      The product description for the card. Used to be shown
//                      in the UI when card is presented. Added in version 102.
//   card_issuer_id     The id of the card's issuer.
//   virtual_card_enrollment_type
//                      An enum indicating the type of virtual card enrollment
//                      of this card. kTypeUnspecified is the default value.
//                      kIssuer denotes that it is an issuer-level enrollment.
//                      kNetwork denotes that it is a network-level enrollment.
// unmasked_credit_cards
//                      When a masked credit credit card is unmasked and the
//                      full number is downloaded or when the full number is
//                      available upon saving card to server, it will be stored
//                      here.
//
//   id                 Server ID. This can be joined with the id in the
//                      masked_credit_cards table to get the rest of the data.
//   card_number_encrypted
//                      Full card number, encrypted.
//   unmask_date        The date this card was unmasked in units of
//                      Time::ToInternalValue. Added in version 64.
//
// server_card_cloud_token_data
//                      Stores data related to Cloud Primary Account Number
//                      (CPAN) of server credit cards. Each card can have
//                      multiple entries.
//
//   id                 The server ID, which matches an ID from the
//                      masked_credit_cards table.
//   suffix             Last 4-5 digits of the Cloud Primary Account Number.
//   exp_month          Expiration month associated with the CPAN.
//   exp_year           Four-digit Expiration year associated with the CPAN.
//   card_art_url       URL of the card art to be displayed for CPAN.
//   instrument_token   Opaque identifier for the cloud token associated with
//                      the payment instrument.
//
// server_card_metadata
//                      Metadata (currently, usage data) about server credit
//                      cards. This will be synced.
//
//   id                 The server ID, which matches an ID from the
//                      masked_credit_cards table.
//   use_count          The number of times this card has been used to fill
//                      a form.
//   use_date           The date this card was last used to fill a form,
//                      in internal t.
//   billing_address_id The string that identifies the profile which is the
//                      billing address for this card. Can be null in the
//                      database, but always returned as an empty string in
//                      CreditCard. Added in version 71.
//
// local_ibans          This table contains International Bank Account
//                      Numbers (IBANs) added by the user. The columns are
//                      standard entries in an Iban form. Those are local IBANs
//                      and exist on Chrome client only.
//
//   guid               A guid string to uniquely identify the IBAN.
//   use_count          The number of times this IBAN has been used to fill
//                      a form.
//   use_date           The date this IBAN was last used to fill a form,
//                      in time_t.
//   value_encrypted    Actual value of the IBAN (the bank account number),
//                      encrypted.
//   nickname           A nickname for the IBAN, entered by the user.
//
//
// masked_ibans         This table contains "masked" International Bank Account
//                      Numbers (IBANs) added by the user. Those are server
//                      IBANs saved on GPay server and are available across all
//                      the Chrome devices.
//
//   instrument_id      String assigned by the server to identify this IBAN.
//                      This is opaque to the client.
//   prefix             Contains the prefix of the full IBAN value that is
//                      shown when in a masked format.
//   suffix             Contains the suffix of the full IBAN value that is
//                      shown when in a masked format.
//   length             Length of the full IBAN value.
//   nickname           A nickname for the IBAN, entered by the user.
//
// masked_ibans_metadata
//                      Metadata (currently, usage data) about server IBANS.
//                      This will be synced from Chrome sync.
//
//   instrument_id      The instrument ID, which matches an ID from the
//                      masked_ibans table.
//   use_count          The number of times this IBAN has been used to fill
//                      a form.
//   use_date           The date this IBAN was last used to fill a form,
//                      in time_t.
//
// server_addresses     This table contains Autofill address data synced from
//                      the wallet server. It's basically the same as the
//                      autofill_profiles table but locally immutable.
//
//   id                 String assigned by the server to identify this address.
//                      This is opaque to the client.
//   recipient_name     Added in v63.
//   company_name
//   street_address     The combined lines of the street address.
//   address_1          Also known as "administrative area". This is normally
//                      the state or province in most countries.
//   address_2          Also known as "locality". In the US this is the city.
//   address_3          A sub-classification beneath the city, e.g. an
//                      inner-city district or suburb. Also known as
//                      "dependent_locality".
//   address_4          Used in certain countries. Also known as
//                      "sub_dependent_locality".
//   postal_code
//   sorting_code       Similar to the zipcode column, but used for businesses
//                      or organizations that might not be geographically
//                      contiguous. The canonical example is CEDEX in France.
//   country_code
//   language_code      The BCP 47 language code used to format the address for
//                      display. For example, a JP address with "ja" language
//                      code starts with the postal code, but a JP address with
//                      "ja-latn" language code starts with the recipient name.
//   phone_number       Phone number. This is a string and has no formatting
//                      constraints. Added in version 64.
//
// server_address_metadata
//                      Metadata (currently, usage data) about server addresses.
//                      This will be synced.
//
//   id                 The server ID, which matches an ID from the
//                      server_addresses table.
//   use_count          The number of times this address has been used to fill
//                      a form.
//   use_date           The date this address was last used to fill a form,
//                      in internal t.
//   has_converted      Whether this server address has been converted to a
//                      local autofill profile.
//
// autofill_sync_metadata
//                      Sync-specific metadata for autofill records.
//
//   model_type         An int value corresponding to syncer::ModelType enum.
//                      Added in version 78.
//   storage_key        A string that uniquely identifies the metadata record
//                      as well as the corresponding autofill record.
//   value              The serialized EntityMetadata record.
//
// autofill_model_type_state
//                      Contains sync ModelTypeStates for autofill model types.
//
//   model_type         An int value corresponding to syncer::ModelType enum.
//                      Added in version 78. Previously, the table was used only
//                      for one model type, there was an id column with value 1
//                      for the single entry.
//   value              The serialized ModelTypeState record.
//
// payments_customer_data
//                      Contains Google Payments customer data.
//
//   customer_id        A string representing the Google Payments customer id.
//
// offer_data           The data for Autofill offers which will be presented in
//                      payments autofill flows.
//
//   offer_id           The unique server ID for this offer data.
//   offer_reward_amount
//                      The string including the reward details of the offer.
//                      Could be either percentage cashback (XXX%) or fixed
//                      amount cashback (XXX$).
//   expiry             The timestamp when the offer will go expired. Expired
//                      offers will not be shown in the frontend.
//   offer_details_url  The link leading to the offer details page on Gpay app.
//   promo_code         The promo code to be autofilled for a promo code offer.
//   value_prop_text    Server-driven UI string to explain the value of the
//                      offer.
//   see_details_text   Server-driven UI string to imply or link additional
//                      details.
//   usage_instructions_text
//                      Server-driven UI string to instruct the user on how they
//                      can redeem the offer.
//
// offer_eligible_instrument
//                      Contains the mapping of credit cards and card linked
//                      offers.
//
//   offer_id           Int 64 to identify the relevant offer. Matches the
//                      offer_id in the offer_data table.
//   instrument_id      The new form of instrument id of the card. Will not be
//                      used for now.
//
// offer_merchant_domain
//                      Contains the mapping of merchant domains and card linked
//                      offers.
//
//   offer_id           Int 64 to identify the relevant offer. Matches the
//                      offer_id in the offer_data table.
//   merchant_domain    List of full origins for merchant websites on which
//                      this offer would apply.
//
// contact_info         This table contains Autofill profile data synced from a
//                      remote source.
// local_addresses      This table contains kLocalOrSyncable Autofill profiles.
//                      It has the same layout as the contact_info table.
//
//   guid               A guid string to uniquely identify the profile.
//   use_count          The number of times this profile has been used to fill a
//                      form.
//   use_date           The date this profile was last used to fill a form, in
//                      time_t.
//   date_modified      The date on which this profile was last modified, in
//                      time_t.
//   language_code      The BCP 47 language code used to format the address for
//                      display. For example, a JP address with "ja" language
//                      code starts with the postal code, but a JP address with
//                      "ja-latn" language code starts with the recipient name.
//   label              A user-chosen and user-visible label for the profile to
//                      help identifying the semantics of the profile. The user
//                      can choose an arbitrary string in principle, but the
//                      values '$HOME$' and '$WORK$' indicate a special meaning.
//   initial_creator_id The application that initially created the profile.
//                      Represented as an integer. See AutofillProfile.
//   last_modifier_id   The application that performed the last non-metadata
//                      modification of the profile.
//                      Represented as an integer. See AutofillProfile.
//
// contact_info_type_tokens
//                      Contains the values for all relevant ServerFieldTypes of
//                      a contact_info entry. At most one entry per (guid, type)
//                      pair exists.
// local_addresses_type_tokens
//                      Like contact_info_type_tokens, but for local_addresses.
//
//  guid                The guid of the corresponding profile in contact_info.
//  type                The ServerFieldType, represented by its integer value in
//                      the ServerFieldType enum.
//  value               The string value of the type.
//  verification_status Each token has an additional validation status that
//                      indicates if Autofill parsed the value out of an
//                      unstructured token, or if Autofill formatted the token
//                      from a structured subcomponent, or if the value was
//                      observed in a form submission, or even validated by the
//                      user in the settings.
//  observations        An encoding of the observations stored for this `type`.
//                      See `ProfileTokenConfidence::
//                      SerializeObservationsForStoredType()`.
//
// virtual_card_usage_data
//                      Contains data related to retrieval attempts of a virtual
//                      card on a particular merchant domain
//
//  id                  Unique identifier for retrieval data. Generated
//                      originally in chrome sync server.
//  instrument_id       The instrument id of the actual card that the virtual
//                      card is related to.
//  merchant_domain     The merchant domain the usage data is linked to.
//  last_four           The last four digits of the virtual card number. This is
//                      tied to the usage data because the virtual card number
//                      may vary depending on merchants.
//
// local_stored_cvc     This table contains credit card CVC data stored locally
//                      in Chrome.
//
//  guid                A guid string to identify the corresponding locally
//                      stored credit card in the credit_cards table.
//  value_encrypted     Encrypted CVC value of the card. May be 3 digits or 4
//                      digits depending on the card issuer.
//  last_updated_timestamp
//                      The timestamp of the most recent update to the data
//                      entry.
//
// server_stored_cvc    This table contains credit card CVC data stored synced
//                      to Chrome Sync's Kansas server.
//
//  instrument_id       A server generated id to identify the corresponding
//                      credit cards stored in the masked_credit_cards table.
//  value_encrypted     Encrypted CVC value of the card. May be 3 digits or 4
//                      digits depending on the card issuer.
//  last_updated_timestamp
//                      The timestamp of the most recent update to the data
//                      entry.
//
// payment_instruments  This table contains basic details that apply to all
//                      payment instruments synced from Payments backend via
//                      Chrome Sync. This does not apply to credit cards or IBAN
//                      for legacy reasons.
//                      The pair of (`instrument_id`, `instrument_type`) are the
//                      composite primary key for this table
//
//  instrument_id       The server-generated id for the payment instrument.
//  instrument_type     The type of payment instrument. This is an integer
//                      mapping to one of the following types: {BankAccount}.
//                      This determines which table to query for fetching the
//                      instrument details.
//  nickname            The nickname set by the user for the payment instrument.
//  display_icon_url    The URL for the icon to be displayed when showing the
//                      payment instrument to the user.
//
// payment_instruments_metadata
//                      Metadata (currently, usage data) about payment
//                      instruments. This will be synced.
//                      The pair of (`instrument_id`, `instrument_type`) are the
//                      composite primary key for this table and can be used as
//                      the foreign key to the `payment_instruments` table.
//
//  instrument_id       The server-generated id for the payment instrument.
//  instrument_type     The type of payment instrument. This is an integer
//                      mapping to one of the following types: {BankAccount}.
//  use_count           The number of times this payment instrument has been
//                      used.
//  use_date            The date this payment instrument was last used.
//
// payment_instrument_supported_rails
//                      This table stores the mapping of what payment instrument
//                      is supported for which payment rails, where a rail can
//                      loosely represent the different ways in which Chrome can
//                      intercept a user's payment journey and assist in
//                      completing it. For example: Pix, UPI, Card number, IBAN
//                      etc.
//                      The tuple of (`instrument_id`, `instrument_type`,
//                      `payment_rail`) are the composite primary key for this
//                      table. The pair of can (`instrument_id`,
//                      `instrument_type`) can be used as foreign key to the
//                      `payment_instruments` table.
//
//  instrument_id       The server-generated id for the payment instrument.
//  instrument_type     The type of payment instrument. This is an integer
//                      mapping to one of the following types: {BankAccount}.
//  payment_rail        This is an integer mapping to one of the following
//                      types: {Pix}.
//
// bank_accounts        This table contains the bank account data synced via
//                      Chrome Sync.
//
//  instrument_id       The identifier assigned by the GPay server to this bank
//                      account. This is intended to be a unique field.
//  bank_name           The name of the bank where the account is registered.
//  account_number_suffix
//                      The last four digits of the bank account, with which the
//                      user can identify the account.
//  account_type        The type of bank account. This is an integer mapping to
//                      one of the following types: {Checking, Savings, Current,
//                      Salary, Transacting}
//
class AutofillTable : public WebDatabaseTable,
                      public syncer::SyncMetadataStore {
 public:
  AutofillTable();

  AutofillTable(const AutofillTable&) = delete;
  AutofillTable& operator=(const AutofillTable&) = delete;

  ~AutofillTable() override;

  // Retrieves the AutofillTable* owned by |db|.
  static AutofillTable* FromWebDatabase(WebDatabase* db);

  // All ServerFieldTypes stored for an AutofillProfile in the local_addresses
  // or contact_info table (depending on the profile source).
  // When introducing a new field type, it suffices to add it here. When
  // removing a field type, removing it from the list suffices (no additional
  // clean-up in the table necessary). This is not reusing
  // `AutofillProfile::SupportedTypes()` for three reasons:
  // - Due to the table design, the stored types are already ambiguous, so we
  //   prefer the explicitness here.
  // - Some supported types (like PHONE_HOME_CITY_CODE) are not stored.
  // - Some non-supported types are stored (usually types that don't have
  //   filling support yet).
  static base::span<const ServerFieldType> GetStoredTypesForAutofillProfile();

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Records the form elements in |elements| in the database in the
  // autofill table.  A list of all added and updated autofill entries
  // is returned in the changes out parameter.
  bool AddFormFieldValues(const std::vector<FormFieldData>& elements,
                          std::vector<AutocompleteChange>* changes);

  // Records a single form element in the database in the autofill table. A list
  // of all added and updated autocomplete entries is returned in the changes
  // out parameter.
  bool AddFormFieldValue(const FormFieldData& element,
                         std::vector<AutocompleteChange>* changes);

  // Retrieves a vector of all values which have been recorded in the autofill
  // table as the value in a form element with name |name| and which start with
  // |prefix|. The comparison of the prefix is case insensitive.
  bool GetFormValuesForElementName(const std::u16string& name,
                                   const std::u16string& prefix,
                                   std::vector<AutocompleteEntry>* entries,
                                   int limit);

  // Removes rows from the autofill table if they were created on or after
  // |delete_begin| and last used strictly before |delete_end|. For rows where
  // the time range [date_created, date_last_used] overlaps with [delete_begin,
  // delete_end), but is not entirely contained within the latter range, updates
  // the rows so that their resulting time range [new_date_created,
  // new_date_last_used] lies entirely outside of [delete_begin, delete_end),
  // updating the count accordingly. A list of all changed keys and whether
  // each was updater or removed is returned in the changes out parameter.
  bool RemoveFormElementsAddedBetween(const base::Time& delete_begin,
                                      const base::Time& delete_end,
                                      std::vector<AutocompleteChange>* changes);

  // Removes rows from the autofill table if they were last accessed strictly
  // before |AutocompleteEntry::ExpirationTime()|.
  bool RemoveExpiredFormElements(std::vector<AutocompleteChange>* changes);

  // Removes the row from the autofill table for the given |name| |value| pair.
  virtual bool RemoveFormElement(const std::u16string& name,
                                 const std::u16string& value);

  // Returns the number of unique values such that for all autofill entries with
  // that value, the interval between creation date and last usage is entirely
  // contained between [|begin|, |end|).
  virtual int GetCountOfValuesContainedBetween(const base::Time& begin,
                                               const base::Time& end);

  // Retrieves all of the entries in the autofill table.
  virtual bool GetAllAutocompleteEntries(
      std::vector<AutocompleteEntry>* entries);

  // Retrieves a single entry from the autofill table.
  virtual bool GetAutofillTimestamps(const std::u16string& name,
                                     const std::u16string& value,
                                     base::Time* date_created,
                                     base::Time* date_last_used);

  // Replaces existing autocomplete entries with the entries supplied in
  // the argument. If the entry does not already exist, it will be added.
  virtual bool UpdateAutocompleteEntries(
      const std::vector<AutocompleteEntry>& entries);

  // Records a single Autofill profile in the autofill_profiles table.
  virtual bool AddAutofillProfile(const AutofillProfile& profile);

  // Updates the database values for the specified profile.  Multi-value aware.
  virtual bool UpdateAutofillProfile(const AutofillProfile& profile);

  // Removes the Autofill profile with the given `guid`. `profile_source`
  // indicates where the profile was synced from and thus whether it is stored
  // in `kAutofillProfilesTable` or `kContactInfoTable`.
  virtual bool RemoveAutofillProfile(const std::string& guid,
                                     AutofillProfile::Source profile_source);

  // Removes all profiles from the given `profile_source`.
  bool RemoveAllAutofillProfiles(AutofillProfile::Source profile_source);

  // Retrieves a profile with guid `guid` from `kAutofillProfilesTable` or
  // `kContactInfoTable`.
  std::unique_ptr<AutofillProfile> GetAutofillProfile(
      const std::string& guid,
      AutofillProfile::Source profile_source) const;

  // Retrieves local/server profiles in the database. They are returned in
  // unspecified order.
  // The `profile_source` specifies if profiles from the legacy or the remote
  // backend should be retrieved.
  virtual bool GetAutofillProfiles(
      AutofillProfile::Source profile_source,
      std::vector<std::unique_ptr<AutofillProfile>>* profiles) const;
  virtual bool GetServerProfiles(
      std::vector<std::unique_ptr<AutofillProfile>>* profiles) const;

  // Sets the server profiles. All old profiles are deleted and replaced with
  // the given ones.
  void SetServerProfiles(const std::vector<AutofillProfile>& profiles);

  // Records a single BankAccount in the bank accounts table. Returns true if
  // the BankAccount was successfully added to the database.
  bool AddBankAccount(const BankAccount& bank_account);
  // Returns true if the BankAccount was successfully updated in the database.
  bool UpdateBankAccount(const BankAccount& bank_account);
  // Delete the bank account from the database.
  bool RemoveBankAccount(int64_t instrument_id);

  // Records a single IBAN in the local_ibans table.
  bool AddLocalIban(const Iban& iban);

  // Updates the database values for the specified IBAN.
  bool UpdateLocalIban(const Iban& iban);

  // Removes a row from the local_ibans table. `guid` is the identifier of the
  // IBAN to remove.
  bool RemoveLocalIban(const std::string& guid);

  // Retrieves an IBAN with the given `guid`.
  std::unique_ptr<Iban> GetLocalIban(const std::string& guid);

  // Retrieves the local IBANs in the database.
  bool GetLocalIbans(std::vector<std::unique_ptr<Iban>>* ibans);

  // Records a single credit card in the credit_cards table.
  bool AddCreditCard(const CreditCard& credit_card);

  // Updates the database values for the specified credit card.
  bool UpdateCreditCard(const CreditCard& credit_card);

  // Update the CVC in the `kLocalStoredCvcTable` for the given `guid`. Return
  // value indicates if `kLocalStoredCvcTable` got updated or not.
  bool UpdateLocalCvc(const std::string& guid, const std::u16string& cvc);

  // Removes a row from the credit_cards table.  |guid| is the identifier of the
  // credit card to remove.
  bool RemoveCreditCard(const std::string& guid);

  // Adds to the masked_credit_cards and unmasked_credit_cards tables.
  bool AddFullServerCreditCard(const CreditCard& credit_card);

  // Retrieves a credit card with guid |guid|.
  std::unique_ptr<CreditCard> GetCreditCard(const std::string& guid);

  // Retrieves the local/server credit cards in the database.
  virtual bool GetCreditCards(
      std::vector<std::unique_ptr<CreditCard>>* credit_cards);
  virtual bool GetServerCreditCards(
      std::vector<std::unique_ptr<CreditCard>>* credit_cards) const;

  // Replaces all server credit cards with the given vector. Unmasked cards
  // present in the new list will be preserved (even if the input is MASKED).
  void SetServerCreditCards(const std::vector<CreditCard>& credit_cards);

  // Cards synced from the server may be "masked" (only last 4 digits
  // available) or "unmasked" (everything is available). These functions set
  // that state.
  bool UnmaskServerCreditCard(const CreditCard& masked,
                              const std::u16string& full_number);
  bool MaskServerCreditCard(const std::string& id);

  // Methods to add, update, remove, clear and get cvc in the
  // `server_stored_cvc` table. Return value indicates if the operation is
  // succeeded and value actually changed. It may return false when operation is
  // success but no data is changed, e.g. delete an empty table.
  bool AddServerCvc(const ServerCvc& server_cvc);
  bool UpdateServerCvc(const ServerCvc& server_cvc);
  bool RemoveServerCvc(int64_t instrument_id);
  // This will clear all server cvcs.
  bool ClearServerCvcs();
  // When a server card is deleted on payment side (i.e. pay.google.com), sync
  // notifies Chrome about the card deletion, not the CVC deletion, as Payment
  // server does not have access to the CVC storage on the Sync server side, so
  // Payment cannot delete the CVC from server side directly, and this has to be
  // done on the Chrome side. So this ReconcileServerCvc will be invoked when
  // card sync happens and will remove orphaned CVC from the current client.
  bool ReconcileServerCvcs();
  // Get all server cvcs from `server_stored_cvc` table.
  std::vector<std::unique_ptr<ServerCvc>> GetAllServerCvcs() const;

  // Methods to add, update, remove and get the metadata for server cards,
  // addresses, and IBANs. Return true if the operations succeeded.
  bool AddServerCardMetadata(const AutofillMetadata& card_metadata);
  bool UpdateServerCardMetadata(const CreditCard& credit_card);
  bool UpdateServerCardMetadata(const AutofillMetadata& card_metadata);
  bool RemoveServerCardMetadata(const std::string& id);
  bool GetServerCardsMetadata(
      std::map<std::string, AutofillMetadata>* cards_metadata) const;
  bool AddServerAddressMetadata(const AutofillMetadata& address_metadata);
  bool UpdateServerAddressMetadata(const AutofillProfile& profile);
  bool UpdateServerAddressMetadata(const AutofillMetadata& address_metadata);
  bool RemoveServerAddressMetadata(const std::string& id);
  bool GetServerAddressesMetadata(
      std::map<std::string, AutofillMetadata>* addresses_metadata) const;
  bool AddOrUpdateServerIbanMetadata(const Iban& iban);
  bool RemoveServerIbanMetadata(const std::string& instrument_id);
  std::vector<AutofillMetadata> GetServerIbansMetadata() const;

  // Methods to add the server cards and addresses data independently from the
  // metadata.
  void SetServerCardsData(const std::vector<CreditCard>& credit_cards);
  void SetServerAddressesData(const std::vector<AutofillProfile>& profiles);

  // Setters and getters related to the CreditCardCloudTokenData of server
  // cards. Used by AutofillWalletSyncBridge to interact with the stored data.
  void SetCreditCardCloudTokenData(const std::vector<CreditCardCloudTokenData>&
                                       credit_card_cloud_token_data);
  bool GetCreditCardCloudTokenData(
      std::vector<std::unique_ptr<CreditCardCloudTokenData>>*
          credit_card_cloud_token_data);

  // Gets the list of server IBANs from the database.
  std::vector<std::unique_ptr<Iban>> GetServerIbans();
  // Overwrite the IBANs in the database with the given `ibans`.
  bool SetServerIbans(const std::vector<Iban>& ibans);

  // Setters and getters related to the Google Payments customer data.
  // Passing null to the setter will clear the data.
  void SetPaymentsCustomerData(const PaymentsCustomerData* customer_data);
  // Getter returns false if it could not execute the database statement, and
  // may return true but leave |customer_data| untouched if there is no data.
  bool GetPaymentsCustomerData(
      std::unique_ptr<PaymentsCustomerData>* customer_data) const;

  // |autofill_offer_data| must include all existing offers, since table will
  // be completely overwritten.
  void SetAutofillOffers(
      const std::vector<AutofillOfferData>& autofill_offer_data);
  bool GetAutofillOffers(
      std::vector<std::unique_ptr<AutofillOfferData>>* autofill_offer_data);

  // CRUD operations for VirtualCardUsageData in the virtual_card_usage_data
  // table
  bool AddOrUpdateVirtualCardUsageData(
      const VirtualCardUsageData& virtual_card_usage_data);
  std::unique_ptr<VirtualCardUsageData> GetVirtualCardUsageData(
      const std::string& usage_data_id);
  bool RemoveVirtualCardUsageData(const std::string& usage_data_id);
  void SetVirtualCardUsageData(
      const std::vector<VirtualCardUsageData>& virtual_card_usage_data);
  bool GetAllVirtualCardUsageData(
      std::vector<std::unique_ptr<VirtualCardUsageData>>*
          virtual_card_usage_data);
  bool RemoveAllVirtualCardUsageData();

  // Deletes all data from the server card and profile tables. Returns true if
  // any data was deleted, false if not (so false means "commit not needed"
  // rather than "error").
  bool ClearAllServerData();

  // Deletes all data from the local card and profiles table. Returns true if
  // any data was deleted, false if not (so false means "commit not needed"
  // rather than "error").
  bool ClearAllLocalData();

  // Removes rows from autofill_profiles and credit_cards if they were created
  // on or after `delete_begin` and strictly before `delete_end`. Returns the
  // list of deleted profile guids in `profile_guids`. Return value is true if
  // all rows were successfully removed. Returns false on database error. In
  // that case, the output vector state is undefined, and may be partially
  // filled.
  // TODO(crbug.com/1135188): This function is solely used to remove browsing
  // data. Once explicit save dialogs are fully launched, it can be removed. For
  // this reason profiles in the `contact_info` table are not considered.
  bool RemoveAutofillDataModifiedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      std::vector<std::unique_ptr<AutofillProfile>>* profiles,
      std::vector<std::unique_ptr<CreditCard>>* credit_cards);

  // Removes origin URLs from the credit_cards tables if they were written on or
  // after `delete_begin` and strictly before `delete_end`. Returns true if all
  // rows were successfully updated and false on a database error.
  bool RemoveOriginURLsModifiedBetween(const base::Time& delete_begin,
                                       const base::Time& delete_end);

  // Clear all local payment methods (credit cards and IBANs).
  void ClearLocalPaymentMethodsData();

  // Read all the stored metadata for |model_type| and fill |metadata_batch|
  // with it.
  bool GetAllSyncMetadata(syncer::ModelType model_type,
                          syncer::MetadataBatch* metadata_batch);

  // Deletes all metadata for |model_type|.
  bool DeleteAllSyncMetadata(syncer::ModelType model_type);

  // syncer::SyncMetadataStore implementation.
  bool UpdateEntityMetadata(syncer::ModelType model_type,
                            const std::string& storage_key,
                            const sync_pb::EntityMetadata& metadata) override;
  bool ClearEntityMetadata(syncer::ModelType model_type,
                           const std::string& storage_key) override;
  bool UpdateModelTypeState(
      syncer::ModelType model_type,
      const sync_pb::ModelTypeState& model_type_state) override;
  bool ClearModelTypeState(syncer::ModelType model_type) override;

  // Table migration functions. NB: These do not and should not rely on other
  // functions in this class. The implementation of a function such as
  // GetCreditCard may change over time, but MigrateToVersionXX should never
  // change.
  bool MigrateToVersion83RemoveServerCardTypeColumn();
  bool MigrateToVersion84AddNicknameColumn();
  bool MigrateToVersion85AddCardIssuerColumnToMaskedCreditCard();
  bool MigrateToVersion86RemoveUnmaskedCreditCardsUseColumns();
  bool MigrateToVersion87AddCreditCardNicknameColumn();
  bool MigrateToVersion88AddNewNameColumns();
  bool MigrateToVersion89AddInstrumentIdColumnToMaskedCreditCard();
  bool MigrateToVersion90AddNewStructuredAddressColumns();
  bool MigrateToVersion91AddMoreStructuredAddressColumns();
  bool MigrateToVersion92AddNewPrefixedNameColumn();
  bool MigrateToVersion93AddAutofillProfileLabelColumn();
  bool MigrateToVersion94AddPromoCodeColumnsToOfferData();
  bool MigrateToVersion95AddVirtualCardMetadata();
  bool MigrateToVersion96AddAutofillProfileDisallowConfirmableMergesColumn();
  bool MigrateToVersion98RemoveStatusColumnMaskedCreditCards();
  bool MigrateToVersion99RemoveAutofillProfilesTrashTable();
  bool MigrateToVersion100RemoveProfileValidityBitfieldColumn();
  bool MigrateToVersion101RemoveCreditCardArtImageTable();
  bool MigrateToVersion102AddAutofillBirthdatesTable();
  bool MigrateToVersion104AddProductDescriptionColumn();
  bool MigrateToVersion105AddAutofillIbanTable();
  bool MigrateToVersion106RecreateAutofillIbanTable();
  bool MigrateToVersion107AddContactInfoTables();
  bool MigrateToVersion108AddCardIssuerIdColumn();
  bool MigrateToVersion109AddVirtualCardUsageDataTable();
  bool MigrateToVersion110AddInitialCreatorIdAndLastModifierId();
  bool MigrateToVersion111AddVirtualCardEnrollmentTypeColumn();
  // No MigrateToVersion112. WebDatabase changed, but AutofillTable wasn't
  // affected.
  bool MigrateToVersion113MigrateLocalAddressProfilesToNewTable();
  bool MigrateToVersion114DropLegacyAddressTables();
  bool MigrateToVersion115EncryptIbanValue();
  bool MigrateToVersion116AddStoredCvcTable();
  bool MigrateToVersion117AddProfileObservationColumn();
  bool MigrateToVersion118RemovePaymentsUpiVpaTable();
  bool MigrateToVersion119AddMaskedIbanTablesAndRenameLocalIbanTable();
  bool MigrateToVersion120AddPaymentInstrumentAndBankAccountTables();

  // Max data length saved in the table, AKA the maximum length allowed for
  // form data.
  // Copied to components/autofill/ios/browser/resources/autofill_controller.js.
  static const size_t kMaxDataLength;

 private:
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autocomplete);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autocomplete_AddChanges);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           Autocomplete_GetCountOfValuesContainedBetween);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           Autocomplete_RemoveBetweenChanges);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autocomplete_UpdateDontReplace);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autocomplete_RemoveFormElementsAddedBetween_UsedOnlyBefore);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autocomplete_RemoveFormElementsAddedBetween_UsedOnlyAfter);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autocomplete_RemoveFormElementsAddedBetween_UsedOnlyDuring);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autocomplete_RemoveFormElementsAddedBetween_UsedBeforeAndDuring);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autocomplete_RemoveFormElementsAddedBetween_UsedDuringAndAfter);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autocomplete_RemoveFormElementsAddedBetween_OlderThan30Days);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           RemoveExpiredFormElements_Expires_DeleteEntry);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           RemoveExpiredFormElements_NotOldEnough);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autocomplete_AddFormFieldValues);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, AutofillProfile);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, UpdateAutofillProfile);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           RemoveAutofillDataModifiedBetween);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, CreditCard);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, UpdateCreditCard);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           Autocomplete_GetAllAutocompleteEntries_OneResult);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           Autocomplete_GetAllAutocompleteEntries_TwoDistinct);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           Autocomplete_GetAllAutocompleteEntries_TwoSame);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autocomplete_GetEntry_Populated);

  // Methods for adding autocomplete entries at a specified time. For testing
  // only.
  bool AddFormFieldValuesTime(const std::vector<FormFieldData>& elements,
                              std::vector<AutocompleteChange>* changes,
                              base::Time time);
  bool AddFormFieldValueTime(const FormFieldData& element,
                             std::vector<AutocompleteChange>* changes,
                             base::Time time);

  bool SupportsMetadataForModelType(syncer::ModelType model_type) const;
  int GetKeyValueForModelType(syncer::ModelType model_type) const;

  bool GetAllSyncEntityMetadata(syncer::ModelType model_type,
                                syncer::MetadataBatch* metadata_batch);

  bool GetModelTypeState(syncer::ModelType model_type,
                         sync_pb::ModelTypeState* state);

  // Insert a single AutocompleteEntry into the autofill table.
  bool InsertAutocompleteEntry(const AutocompleteEntry& entry);

  // Adds to |masked_credit_cards| and updates |server_card_metadata|.
  // Must already be in a transaction.
  void AddMaskedCreditCards(const std::vector<CreditCard>& credit_cards);

  // Adds to |unmasked_credit_cards|.
  void AddUnmaskedCreditCard(const std::string& id,
                             const std::u16string& full_number);

  // Deletes server credit cards by |id|. Returns true if a row was deleted.
  bool DeleteFromMaskedCreditCards(const std::string& id);
  bool DeleteFromUnmaskedCreditCards(const std::string& id);

  // Helper function extracting common code between `SetServerProfiles()` and
  // `SetServerAddressData()`.
  void SetServerProfilesAndMetadata(
      const std::vector<AutofillProfile>& profiles,
      bool update_metadata);

  // Reads profiles from the deprecated autofill_profiles table.
  std::unique_ptr<AutofillProfile> GetAutofillProfileFromLegacyTable(
      const std::string& guid) const;
  bool GetAutofillProfilesFromLegacyTable(
      std::vector<std::unique_ptr<AutofillProfile>>* profiles) const;

  bool InitMainTable();
  bool InitCreditCardsTable();
  bool InitLocalIbansTable();
  bool InitLegacyProfilesTable();
  bool InitLegacyProfileAddressesTable();
  bool InitLegacyProfileNamesTable();
  bool InitLegacyProfileEmailsTable();
  bool InitLegacyProfilePhonesTable();
  bool InitLegacyProfileBirthdatesTable();
  bool InitMaskedCreditCardsTable();
  bool InitMaskedIbansTable();
  bool InitMaskedIbansMetadataTable();
  bool InitUnmaskedCreditCardsTable();
  bool InitServerCardMetadataTable();
  bool InitServerAddressesTable();
  bool InitServerAddressMetadataTable();
  bool InitAutofillSyncMetadataTable();
  bool InitModelTypeStateTable();
  bool InitPaymentsCustomerDataTable();
  bool InitServerCreditCardCloudTokenDataTable();
  bool InitStoredCvcTable();
  bool InitOfferDataTable();
  bool InitOfferEligibleInstrumentTable();
  bool InitOfferMerchantDomainTable();
  bool InitProfileMetadataTable(AutofillProfile::Source source);
  bool InitProfileTypeTokensTable(AutofillProfile::Source source);
  bool InitVirtualCardUsageDataTable();
  bool InitBankAccountsTable();
  bool InitPaymentInstrumentsTable();
  bool InitPaymentInstrumentsMetadataTable();
  bool InitPaymentInstrumentSupportedRailsTable();

  std::unique_ptr<AutofillTableEncryptor> autofill_table_encryptor_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_H_
