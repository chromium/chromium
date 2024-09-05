// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PAYMENTS_PAYMENTS_AUTOFILL_TABLE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PAYMENTS_PAYMENTS_AUTOFILL_TABLE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

namespace base {
class Time;
}

namespace autofill {

class AutofillOfferData;
class BankAccount;
class CreditCard;
struct CreditCardCloudTokenData;
class Iban;
struct PaymentsCustomerData;
struct PaymentsMetadata;
class VirtualCardUsageData;
// Helper struct to better group server cvc related variables for better
// passing last_updated_timestamp, which is needed for sync bridge. Limited
// scope in autofill table & sync bridge.
struct ServerCvc {
  bool operator==(const ServerCvc&) const = default;
  // A server generated id to identify the corresponding credit card.
  int64_t instrument_id;
  // CVC value of the card.
  std::u16string cvc;
  // The timestamp of the most recent update to the data entry.
  base::Time last_updated_timestamp;
};

// This class manages the various payments Autofill tables within the SQLite
// database passed to the constructor. It expects the following schemas:
//
// Note: The database stores time in seconds, UTC.
// -----------------------------------------------------------------------------
// credit_cards         This table contains credit card data added by the user
//                      with the Autofill dialog.  Most of the columns are
//                      standard entries in a credit card form.
//
//   guid               A guid string to uniquely identify the credit card.
//                      Added in version 31.
//   name_on_card       The cardholder's name, if available.
//   expiration_month   Expiration month: 1-12
//   expiration_year    Four-digit year: 2017
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
// -----------------------------------------------------------------------------
// masked_credit_cards
//                      This table contains "masked" credit card information
//                      about credit cards stored on the server. It consists
//                      of a short description and an ID, but not full payment
//                      information. Writing to this table is done by sync and
//                      on successful save of card to the server.
//
//   id                 String assigned by the server to identify this card.
//                      This is a legacy version of instrument_id and is opaque
//                      to the client.
//   status             Server's status of this card.
//                      TODO(brettw) define constants for this.
//   name_on_card       The cardholder's name, if available.
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
//   product_terms_url  Issuer terms of service to be displayed on the settings
//                      page.
// -----------------------------------------------------------------------------
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
// -----------------------------------------------------------------------------
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
// -----------------------------------------------------------------------------
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
// -----------------------------------------------------------------------------
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
//   nickname           A nickname for the IBAN, entered by the user.
// -----------------------------------------------------------------------------
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
// -----------------------------------------------------------------------------
// payments_customer_data
//                      Contains Google Payments customer data.
//
//   customer_id        A string representing the Google Payments customer id.
// -----------------------------------------------------------------------------
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
// -----------------------------------------------------------------------------
// offer_eligible_instrument
//                      Contains the mapping of credit cards and card linked
//                      offers.
//
//   offer_id           Int 64 to identify the relevant offer. Matches the
//                      offer_id in the offer_data table.
//   instrument_id      The new form of instrument id of the card. Will not be
//                      used for now.
// -----------------------------------------------------------------------------
// offer_merchant_domain
//                      Contains the mapping of merchant domains and card linked
//                      offers.
//
//   offer_id           Int 64 to identify the relevant offer. Matches the
//                      offer_id in the offer_data table.
//   merchant_domain    List of full origins for merchant websites on which
//                      this offer would apply.
// -----------------------------------------------------------------------------
// virtual_card_usage_data
//                      Contains data related to retrieval attempts of a virtual
//                      card on a particular merchant domain
//
//   id                 Unique identifier for retrieval data. Generated
//                      originally in chrome sync server.
//   instrument_id      The instrument id of the actual card that the virtual
//                      card is related to.
//   merchant_domain    The merchant domain the usage data is linked to.
//   last_four          The last four digits of the virtual card number. This is
//                      tied to the usage data because the virtual card number
//                      may vary depending on merchants.
// -----------------------------------------------------------------------------
// local_stored_cvc     This table contains credit card CVC data stored locally
//                      in Chrome.
//
//   guid               A guid string to identify the corresponding locally
//                      stored credit card in the credit_cards table.
//   value_encrypted    Encrypted CVC value of the card. May be 3 digits or 4
//                      digits depending on the card issuer.
//   last_updated_timestamp
//                      The timestamp of the most recent update to the data
//                      entry.
// -----------------------------------------------------------------------------
// server_stored_cvc    This table contains credit card CVC data stored synced
//                      to Chrome Sync's Kansas server.
//
//   instrument_id      A server generated id to identify the corresponding
//                      credit cards stored in the masked_credit_cards table.
//   value_encrypted    Encrypted CVC value of the card. May be 3 digits or 4
//                      digits depending on the card issuer.
//   last_updated_timestamp
//                      The timestamp of the most recent update to the data
//                      entry.
// -----------------------------------------------------------------------------
// masked_bank_accounts_metadata
//                      Metadata (currently, usage data) about masked bank
//                      accounts. This will be synced. This is not part of the
//                      `masked_bank_accounts` table as that table is deleted
//                      and recreated on every sync where as this table is
//                      simply updated after a sync.
//
//   instrument_id      The server-generated id for the bank account.
//   use_count          The number of times this bank account has been used.
//   use_date           The date this bank account was last used.
// -----------------------------------------------------------------------------
// masked_bank_accounts This table contains the bank account data synced via
//                      Chrome Sync.
//
//   instrument_id      The identifier assigned by the GPay server to this bank
//                      account. This is intended to be a unique field.
//   bank_name          The name of the bank where the account is registered.
//   account_number_suffix
//                      The last four digits of the bank account, with which the
//                      user can identify the account.
//   account_type       The type of bank account. This is an integer mapping to
//                      one of the following types: {Checking, Savings, Current,
//                      Salary, Transacting}
//   nickname           The nickname set by the user for the payment instrument.
//   display_icon_url   The URL for the icon to be displayed when showing the
//                      payment instrument to the user.
// -----------------------------------------------------------------------------
// masked_credit_card_benefits
//                      This table contains the multi-valued benefits fields
//                      associated with a credit card, i.e., credit-card-linked
//                      benefits that help users save money on online purchases.
//
//   benefit_id         The unique ID for this benefit data. Generated
//                      originally in Chrome Sync server.
//   instrument_id      The instrument id string that identifies the credit card
//                      to which the benefit belongs to. Identical to
//                      `instrument_id` field in `masked_credit_cards`.
//   benefit_type       The type of benefit. Either category, merchant, or flat
//                      rate.
//   benefit_category   The category that the benefit applies to. Only set when
//                      `benefit_type` == category.
//   benefit_description
//                      A description of what the credit card benefit offers the
//                      user for purchases. Shown in the Autofill suggestion UI.
//   start_time         Timestamp when the benefit is active and should be
//                      displayed. Empty if no time range is specific for the
//                      benefit.
//   end_time           Timestamp When the benefit is no longer active and
//                      should no longer be displayed. This field is only set
//                      for benefits with an expiration date. Empty if the
//                      benefit will last indefinitely.
// -----------------------------------------------------------------------------
// benefit_merchant_domains
//                      Contains the mapping of non-personalized credit card
//                      merchant benefits to eligible merchant domains. A
//                      benefit may apply to multiple domains (and span
//                      multiple rows in this table).
//
//   benefit_id         Unique ID to identify the relevant benefit. Matches the
//                      `benefit_id` in the `masked_credit_card_benefits` table.
//   merchant_domain    Origin for merchant websites on which this benefit
//                      would apply.
// -----------------------------------------------------------------------------
// generic_payment_instruments
//                      Contains serialized versions of payment instruments such
//                      as eWallets.
//
//   instrument_id      The server-generated ID for the payment instrument.
//   serialized_value_encrypted
//                      A byte-encoded representation of the payment
//                      instrument's protobuf, encrypted.
// -----------------------------------------------------------------------------
class PaymentsAutofillTable : public WebDatabaseTable {
 public:
  PaymentsAutofillTable();

  PaymentsAutofillTable(const PaymentsAutofillTable&) = delete;
  PaymentsAutofillTable& operator=(const PaymentsAutofillTable&) = delete;

  ~PaymentsAutofillTable() override;

  // Retrieves the PaymentsAutofillTable* owned by |db|.
  static PaymentsAutofillTable* FromWebDatabase(WebDatabase* db);

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Rewrites the bank accounts table. Returns true if all bank accounts were
  // successfully added to the database.
  bool SetMaskedBankAccounts(const std::vector<BankAccount>& bank_accounts);
  // Retrieve all bank accounts from the database.
  bool GetMaskedBankAccounts(std::vector<BankAccount>& bank_accounts);

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

  // Adds to the masked_credit_cards table. Only tests add cards this way - in
  // production server cards are set directly via `SetServerCreditCards`.
  bool AddServerCreditCardForTesting(const CreditCard& credit_card);

  // Retrieves a credit card with guid |guid|.
  std::unique_ptr<CreditCard> GetCreditCard(const std::string& guid);

  // Retrieves the local/server credit cards in the database.
  virtual bool GetCreditCards(
      std::vector<std::unique_ptr<CreditCard>>* credit_cards);
  virtual bool GetServerCreditCards(
      std::vector<std::unique_ptr<CreditCard>>& credit_cards) const;

  // Replaces all server credit cards with the given vector.
  void SetServerCreditCards(const std::vector<CreditCard>& credit_cards);

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
  // The list of deleted CVCs will also be returned back to trigger the deletion
  // flow for the sync server.
  std::vector<std::unique_ptr<ServerCvc>> DeleteOrphanedServerCvcs();
  // Get all server cvcs from `server_stored_cvc` table.
  std::vector<std::unique_ptr<ServerCvc>> GetAllServerCvcs() const;

  // This will clear all the local cvcs.
  bool ClearLocalCvcs();

  // Methods to add, update, remove and get the metadata for server cards and
  // IBANs.
  // For get method, return true if the operations succeeded.
  // For add/update/remove methods, return true if any changes actually
  // occurred.
  // TODO (crbug.com/1504063): Merge Add/UpdateServerCardMetadata into a single
  // method AddOrUpdateServerCardMetadata.
  bool AddServerCardMetadata(const PaymentsMetadata& card_metadata);
  bool UpdateServerCardMetadata(const CreditCard& credit_card);
  bool UpdateServerCardMetadata(const PaymentsMetadata& card_metadata);
  bool RemoveServerCardMetadata(const std::string& id);
  bool GetServerCardsMetadata(
      std::vector<PaymentsMetadata>& cards_metadata) const;
  bool AddOrUpdateServerIbanMetadata(const PaymentsMetadata& iban_metadata);
  bool RemoveServerIbanMetadata(const std::string& instrument_id);
  bool GetServerIbansMetadata(
      std::vector<PaymentsMetadata>& ibans_metadata) const;

  // Method to add the server cards independently from the metadata.
  void SetServerCardsData(const std::vector<CreditCard>& credit_cards);

  // Setters and getters related to the CreditCardCloudTokenData of server
  // cards. Used by AutofillWalletSyncBridge to interact with the stored data.
  void SetCreditCardCloudTokenData(const std::vector<CreditCardCloudTokenData>&
                                       credit_card_cloud_token_data);
  bool GetCreditCardCloudTokenData(
      std::vector<std::unique_ptr<CreditCardCloudTokenData>>&
          credit_card_cloud_token_data);

  // Returns true if server IBANs are successfully returned via `ibans` from
  // the database.
  bool GetServerIbans(std::vector<std::unique_ptr<Iban>>& ibans);

  // Overwrite the IBANs in the database with the given `ibans`.
  // Note that this method will not update IBAN metadata because that happens in
  // separate flows.
  bool SetServerIbansData(const std::vector<Iban>& ibans);

  // Overwrite the server IBANs and server IBAN metadata with the given `ibans`.
  // This distinction is necessary compared with above method, because metadata
  // and data are synced through separate data types in prod code, while this
  // method is an easy way to set up during tests.
  void SetServerIbansForTesting(const std::vector<Iban>& ibans);

  // Setters and getters related to the Google Payments customer data.
  // Passing null to the setter will clear the data.
  void SetPaymentsCustomerData(const PaymentsCustomerData* customer_data);
  // Getter returns false if it could not execute the database statement, and
  // may return true but leave `customer_data` untouched if there is no data.
  bool GetPaymentsCustomerData(
      std::unique_ptr<PaymentsCustomerData>& customer_data) const;

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
  std::optional<VirtualCardUsageData> GetVirtualCardUsageData(
      const std::string& usage_data_id);
  bool RemoveVirtualCardUsageData(const std::string& usage_data_id);
  void SetVirtualCardUsageData(
      const std::vector<VirtualCardUsageData>& virtual_card_usage_data);
  bool GetAllVirtualCardUsageData(
      std::vector<VirtualCardUsageData>& virtual_card_usage_data);
  bool RemoveAllVirtualCardUsageData();

  // Deletes all data from the server card tables. Returns true if any data was
  // deleted, false if not (so false means "commit not needed" rather than
  // "error").
  bool ClearAllServerData();

  // Set, get, and clear the `credit_card_benefits` table and the
  // 'benefit_merchant_domains' table. Return true if the operation
  // succeeded.
  bool SetCreditCardBenefits(
      const std::vector<CreditCardBenefit>& credit_card_benefits);
  bool GetAllCreditCardBenefits(
      std::vector<CreditCardBenefit>& credit_card_benefits);
  // Get all 'CreditCardBenefit` for the given `instrument_id`. If
  // no `instrument_id` is provided, return all 'CreditCardBenefit`.
  bool GetCreditCardBenefitsForInstrumentId(
      std::optional<int64_t> instrument_id,
      std::vector<CreditCardBenefit>& credit_card_benefits);
  bool ClearAllCreditCardBenefits();

  // Sets and gets the `payment_instruments` table. Return true if the operation
  // succeeded.
  bool SetPaymentInstruments(
      const std::vector<sync_pb::PaymentInstrument>& payment_instruments);
  bool GetPaymentInstruments(
      std::vector<sync_pb::PaymentInstrument>& payment_instruments);

  // Testing helper to access the database for checking the result of database
  // update.
  sql::Database* GetDbForTesting() const { return db(); }

  // Table migration functions. NB: These do not and should not rely on other
  // functions in this class. The implementation of a function such as
  // GetCreditCard may change over time, but MigrateToVersionXX should never
  // change.
  bool MigrateToVersion83RemoveServerCardTypeColumn();
  bool MigrateToVersion84AddNicknameColumn();
  bool MigrateToVersion85AddCardIssuerColumnToMaskedCreditCard();
  bool MigrateToVersion87AddCreditCardNicknameColumn();
  bool MigrateToVersion89AddInstrumentIdColumnToMaskedCreditCard();
  bool MigrateToVersion94AddPromoCodeColumnsToOfferData();
  bool MigrateToVersion95AddVirtualCardMetadata();
  bool MigrateToVersion98RemoveStatusColumnMaskedCreditCards();
  bool MigrateToVersion101RemoveCreditCardArtImageTable();
  bool MigrateToVersion104AddProductDescriptionColumn();
  bool MigrateToVersion105AddAutofillIbanTable();
  bool MigrateToVersion106RecreateAutofillIbanTable();
  bool MigrateToVersion108AddCardIssuerIdColumn();
  bool MigrateToVersion109AddVirtualCardUsageDataTable();
  bool MigrateToVersion111AddVirtualCardEnrollmentTypeColumn();
  bool MigrateToVersion115EncryptIbanValue();
  bool MigrateToVersion116AddStoredCvcTable();
  bool MigrateToVersion118RemovePaymentsUpiVpaTable();
  bool MigrateToVersion119AddMaskedIbanTablesAndRenameLocalIbanTable();
  bool MigrateToVersion123AddProductTermsUrlColumnAndAddCardBenefitsTables();
  bool
  MigrateToVersion124AndDeletePaymentInstrumentRelatedTablesAndAddMaskedBankAccountTable();
  bool MigrateToVersion125DeleteFullServerCardsTable();
  bool MigrateToVersion129AddGenericPaymentInstrumentsTable();
  bool MigrateToVersion131RemoveGenericPaymentInstrumentTypeColumn();
  bool MigrateToVersion133RemoveLengthColumnFromMaskedIbansTable();

 private:
  // Adds to |masked_credit_cards| and updates |server_card_metadata|.
  // Must already be in a transaction.
  void AddMaskedCreditCards(const std::vector<CreditCard>& credit_cards);

  // Deletes server credit cards by |id|. Returns true if a row was deleted.
  bool DeleteFromMaskedCreditCards(const std::string& id);

  // Get the list of eligible merchant domains for the specific 'benefit_id`.
  base::flat_set<url::Origin> GetMerchantDomainsForBenefitId(
      const CreditCardBenefitBase::BenefitId& benefit_id);

  bool InitCreditCardsTable();
  bool InitLocalIbansTable();
  bool InitMaskedCreditCardsTable();
  bool InitMaskedIbansTable();
  bool InitMaskedIbansMetadataTable();
  bool InitUnmaskedCreditCardsTable();
  bool InitServerCardMetadataTable();
  bool InitPaymentsCustomerDataTable();
  bool InitServerCreditCardCloudTokenDataTable();
  bool InitStoredCvcTable();
  bool InitOfferDataTable();
  bool InitOfferEligibleInstrumentTable();
  bool InitOfferMerchantDomainTable();
  bool InitVirtualCardUsageDataTable();
  bool InitMaskedBankAccountsTable();
  bool InitMaskedBankAccountsMetadataTable();
  bool InitMaskedCreditCardBenefitsTable();
  bool InitBenefitMerchantDomainsTable();
  bool InitGenericPaymentInstrumentsTable();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PAYMENTS_PAYMENTS_AUTOFILL_TABLE_H_
