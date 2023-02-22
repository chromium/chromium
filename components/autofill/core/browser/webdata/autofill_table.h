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

#include "base/gtest_prod_util.h"
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

class AutofillChange;
class AutofillEntry;
struct AutofillMetadata;
class AutofillOfferData;
class AutofillTableEncryptor;
class AutofillTableTest;
class CreditCard;
struct CreditCardCloudTokenData;
struct FormFieldData;
class IBAN;
struct PaymentsCustomerData;
class VirtualCardUsageData;

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
// autofill_profile_emails
//                      This table contains the multi-valued email fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which
//                      the email belongs.
//   email
//
// autofill_profile_phones
//                      This table contains the multi-valued phone fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which the
//                      phone number belongs.
//   number
//
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
//                      this card. UNSPECIFIED is the default value. UNENROLLED
//                      means this card has not been enrolled to have virtual
//                      cards. ENROLLED means the card has been enrolled and
//                      has related virtual credit cards.
//   card_art_url       URL to generate the card art image for this card.
//   product_description
//                      The product description for the card. Used to be shown
//                      in the UI when card is presented. Added in version 102.
//   card_issuer_id     The id of the card's issuer.
//   virtual_card_enrollment_type
//                      An enum indicating the type of virtual card enrollment
//                      of this card. TYPE_UNSPECIFIED is the default value.
//                      ISSUER denotes that it is an issuer-level enrollment.
//                      NETWORK denotes that it is a network-level enrollment.
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
// ibans                This table contains International Bank Account
//                      Number(IBAN) data added by the user. The columns are
//                      standard entries in an Iban form.
//
//   guid               A guid string to uniquely identify the IBAN.
//   use_count          The number of times this IBAN has been used to fill
//                      a form.
//   use_date           The date this IBAN was last used to fill a form,
//                      in time_t.
//   value              Actual value of the IBAN (the bank account number).
//   nickname           A nickname for the IBAN, entered by the user.
//
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
// payments_upi_vpa     Contains saved UPI/VPA payment data.
//                      https://en.wikipedia.org/wiki/Unified_Payments_Interface
//
//   vpa                A string representing the UPI ID (a.k.a. VPA) value.
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

class AutofillTable : public WebDatabaseTable,
                      public syncer::SyncMetadataStore {
 public:
  AutofillTable();

  AutofillTable(const AutofillTable&) = delete;
  AutofillTable& operator=(const AutofillTable&) = delete;

  ~AutofillTable() override;

  // Retrieves the AutofillTable* owned by |db|.
  static AutofillTable* FromWebDatabase(WebDatabase* db);

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool IsSyncable() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Records the form elements in |elements| in the database in the
  // autofill table.  A list of all added and updated autofill entries
  // is returned in the changes out parameter.
  bool AddFormFieldValues(const std::vector<FormFieldData>& elements,
                          std::vector<AutofillChange>* changes);

  // Records a single form element in the database in the autofill table. A list
  // of all added and updated autofill entries is returned in the changes out
  // parameter.
  bool AddFormFieldValue(const FormFieldData& element,
                         std::vector<AutofillChange>* changes);

  // Retrieves a vector of all values which have been recorded in the autofill
  // table as the value in a form element with name |name| and which start with
  // |prefix|.  The comparison of the prefix is case insensitive.
  bool GetFormValuesForElementName(const std::u16string& name,
                                   const std::u16string& prefix,
                                   std::vector<AutofillEntry>* entries,
                                   int limit);

  // Removes rows from the autofill table if they were created on or after
  // |delete_begin| and last used strictly before |delete_end|.  For rows where
  // the time range [date_created, date_last_used] overlaps with [delete_begin,
  // delete_end), but is not entirely contained within the latter range, updates
  // the rows so that their resulting time range [new_date_created,
  // new_date_last_used] lies entirely outside of [delete_begin, delete_end),
  // updating the count accordingly.  A list of all changed keys and whether
  // each was updater or removed is returned in the changes out parameter.
  bool RemoveFormElementsAddedBetween(const base::Time& delete_begin,
                                      const base::Time& delete_end,
                                      std::vector<AutofillChange>* changes);

  // Removes rows from the autofill table if they were last accessed strictly
  // before |AutofillEntry::ExpirationTime()|.
  bool RemoveExpiredFormElements(std::vector<AutofillChange>* changes);

  // Removes the row from the autofill table for the given |name| |value| pair.
  virtual bool RemoveFormElement(const std::u16string& name,
                                 const std::u16string& value);

  // Returns the number of unique values such that for all autofill entries with
  // that value, the interval between creation date and last usage is entirely
  // contained between [|begin|, |end|).
  virtual int GetCountOfValuesContainedBetween(const base::Time& begin,
                                               const base::Time& end);

  // Retrieves all of the entries in the autofill table.
  virtual bool GetAllAutofillEntries(std::vector<AutofillEntry>* entries);

  // Retrieves a single entry from the autofill table.
  virtual bool GetAutofillTimestamps(const std::u16string& name,
                                     const std::u16string& value,
                                     base::Time* date_created,
                                     base::Time* date_last_used);

  // Replaces existing autofill entries with the entries supplied in
  // the argument.  If the entry does not already exist, it will be
  // added.
  virtual bool UpdateAutofillEntries(const std::vector<AutofillEntry>& entries);

  // Records a single Autofill profile in the autofill_profiles table.
  virtual bool AddAutofillProfile(const AutofillProfile& profile);

  // Updates the database values for the specified profile.  Multi-value aware.
  virtual bool UpdateAutofillProfile(const AutofillProfile& profile);

  // Removes the Autofill profile with the given `guid`. `profile_source`
  // indicates where the profile was synced from and thus whether it is stored
  // in `kAutofillProfilesTable` or `kContactInfoTable`.
  virtual bool RemoveAutofillProfile(const std::string& guid,
                                     AutofillProfile::Source profile_source);

  // Removes all profiles from the given `profile_source`. Currently this is
  // only supported for kAccount profiles, since they are cleared when the Sync
  // data types gets disabled.
  bool RemoveAllAutofillProfiles(AutofillProfile::Source profile_source);

  // Retrieves a profile with guid `guid` from `kAutofillProfilesTable` or
  // `kContactInfoTable`.
  std::unique_ptr<AutofillProfile> GetAutofillProfile(
      const std::string& guid,
      AutofillProfile::Source profile_source);

  // Retrieves local/server profiles in the database.
  // The `profile_source` specifies if profiles from the legacy or the remote
  // backend should be retrieved.
  virtual bool GetAutofillProfiles(
      std::vector<std::unique_ptr<AutofillProfile>>* profiles,
      AutofillProfile::Source profile_source);
  virtual bool GetServerProfiles(
      std::vector<std::unique_ptr<AutofillProfile>>* profiles) const;

  // Sets the server profiles. All old profiles are deleted and replaced with
  // the given ones.
  void SetServerProfiles(const std::vector<AutofillProfile>& profiles);

  // Records a single IBAN in the iban table.
  bool AddIBAN(const IBAN& iban);

  // Updates the database values for the specified IBAN.
  bool UpdateIBAN(const IBAN& iban);

  // Removes a row from the ibans table. |guid| is the identifier of the
  // IBAN to remove.
  bool RemoveIBAN(const std::string& guid);

  // Retrieves an IBAN with the given |guid|.
  std::unique_ptr<IBAN> GetIBAN(const std::string& guid);

  // Retrieves the local IBANs in the database.
  bool GetIBANs(std::vector<std::unique_ptr<IBAN>>* ibans);

  // Records a single credit card in the credit_cards table.
  bool AddCreditCard(const CreditCard& credit_card);

  // Updates the database values for the specified credit card.
  bool UpdateCreditCard(const CreditCard& credit_card);

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

  // Methods to add, update, remove and get the metadata for server cards and
  // addresses.
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
  bool AddVirtualCardUsageData(
      const VirtualCardUsageData& virtual_card_usage_data);
  bool UpdateVirtualCardUsageData(
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

  // Adds |upi_id| to the saved UPI IDs.
  bool InsertUpiId(const std::string& upi_id);

  // Returns all the UPI IDs stored in the database.
  std::vector<std::string> GetAllUpiIds();

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

  // Removes origin URLs from the autofill_profiles and credit_cards tables if
  // they were written on or after `delete_begin` and strictly before
  // `delete_end`. Returns the list of modified profiles in `profiles`. Return
  // value is true if all rows were successfully updated. Returns false on
  // database error. In that case, the output vector state is undefined, and
  // may be partially filled.
  // Profiles from the `contact_info` table are not considered, as they don't
  // store an origin.
  bool RemoveOriginURLsModifiedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      std::vector<std::unique_ptr<AutofillProfile>>* profiles);

  // Clear all profiles.
  bool ClearAutofillProfiles();

  // Clear all credit cards.
  bool ClearCreditCards();

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

  // Removes the orphan rows in the autofill_profile_names,
  // autofill_profile_emails and autofill_profile_phones table that were not
  // removed in the previous implementation of
  // RemoveAutofillDataModifiedBetween(see crbug.com/836737).
  bool RemoveOrphanAutofillTableRows();

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
  bool MigrateToVersion105AddAutofillIBANTable();
  bool MigrateToVersion106RecreateAutofillIBANTable();
  bool MigrateToVersion107AddContactInfoTables();
  bool MigrateToVersion108AddCardIssuerIdColumn();
  bool MigrateToVersion109AddVirtualCardUsageDataTable();
  bool MigrateToVersion110AddInitialCreatorIdAndLastModifierId();
  bool MigrateToVersion111AddVirtualCardEnrollmentTypeColumn();

  // Max data length saved in the table, AKA the maximum length allowed for
  // form data.
  // Copied to components/autofill/ios/browser/resources/autofill_controller.js.
  static const size_t kMaxDataLength;

 private:
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_AddChanges);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           Autofill_GetCountOfValuesContainedBetween);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_RemoveBetweenChanges);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_UpdateDontReplace);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autofill_RemoveFormElementsAddedBetween_UsedOnlyBefore);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autofill_RemoveFormElementsAddedBetween_UsedOnlyAfter);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autofill_RemoveFormElementsAddedBetween_UsedOnlyDuring);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autofill_RemoveFormElementsAddedBetween_UsedBeforeAndDuring);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autofill_RemoveFormElementsAddedBetween_UsedDuringAndAfter);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autofill_RemoveFormElementsAddedBetween_OlderThan30Days);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           RemoveExpiredFormElements_Expires_DeleteEntry);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           RemoveExpiredFormElements_NotOldEnough);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_AddFormFieldValues);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, AutofillProfile);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, UpdateAutofillProfile);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, AutofillProfileTrash);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, AutofillProfileTrashInteraction);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           RemoveAutofillDataModifiedBetween);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, CreditCard);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, UpdateCreditCard);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           Autofill_GetAllAutofillEntries_OneResult);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           Autofill_GetAllAutofillEntries_TwoDistinct);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           Autofill_GetAllAutofillEntries_TwoSame);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_GetEntry_Populated);

  // Methods for adding autofill entries at a specified time.  For
  // testing only.
  bool AddFormFieldValuesTime(const std::vector<FormFieldData>& elements,
                              std::vector<AutofillChange>* changes,
                              base::Time time);
  bool AddFormFieldValueTime(const FormFieldData& element,
                             std::vector<AutofillChange>* changes,
                             base::Time time);

  bool SupportsMetadataForModelType(syncer::ModelType model_type) const;
  int GetKeyValueForModelType(syncer::ModelType model_type) const;

  bool GetAllSyncEntityMetadata(syncer::ModelType model_type,
                                syncer::MetadataBatch* metadata_batch);

  bool GetModelTypeState(syncer::ModelType model_type,
                         sync_pb::ModelTypeState* state);

  // Insert a single AutofillEntry into the autofill table.
  bool InsertAutofillEntry(const AutofillEntry& entry);

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

  bool InitMainTable();
  bool InitCreditCardsTable();
  bool InitIBANsTable();
  bool InitProfilesTable();
  bool InitProfileAddressesTable();
  bool InitProfileNamesTable();
  bool InitProfileEmailsTable();
  bool InitProfilePhonesTable();
  bool InitProfileBirthdatesTable();
  bool InitMaskedCreditCardsTable();
  bool InitUnmaskedCreditCardsTable();
  bool InitServerCardMetadataTable();
  bool InitServerAddressesTable();
  bool InitServerAddressMetadataTable();
  bool InitAutofillSyncMetadataTable();
  bool InitModelTypeStateTable();
  bool InitPaymentsCustomerDataTable();
  bool InitPaymentsUPIVPATable();
  bool InitServerCreditCardCloudTokenDataTable();
  bool InitOfferDataTable();
  bool InitOfferEligibleInstrumentTable();
  bool InitOfferMerchantDomainTable();
  bool InitContactInfoTable();
  bool InitContactInfoTypeTokensTable();
  bool InitVirtualCardUsageDataTable();

  std::unique_ptr<AutofillTableEncryptor> autofill_table_encryptor_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_H_
