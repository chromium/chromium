// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_table.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_table_encryptor.h"
#include "components/autofill/core/browser/webdata/autofill_table_encryptor_factory.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "url/gurl.h"

namespace autofill {
namespace {

// Helper struct for AutofillTable::RemoveFormElementsAddedBetween().
// Contains all the necessary fields to update a row in the 'autofill' table.
struct AutofillUpdate {
  base::string16 name;
  base::string16 value;
  time_t date_created;
  time_t date_last_used;
  int count;
};

// Returns the |data_model|'s value corresponding to the |type|, trimmed to the
// maximum length that can be stored in a column of the Autofill database.
base::string16 GetInfo(const AutofillDataModel& data_model,
                       ServerFieldType type) {
  base::string16 data = data_model.GetRawInfo(type);
  if (data.size() > AutofillTable::kMaxDataLength)
    return data.substr(0, AutofillTable::kMaxDataLength);

  return data;
}

void BindAutofillProfileToStatement(const AutofillProfile& profile,
                                    const base::Time& modification_date,
                                    sql::Statement* s) {
  DCHECK(base::IsValidGUID(profile.guid()));
  int index = 0;
  s->BindString(index++, profile.guid());

  s->BindString16(index++, GetInfo(profile, COMPANY_NAME));
  s->BindString16(index++, GetInfo(profile, ADDRESS_HOME_STREET_ADDRESS));
  s->BindString16(index++, GetInfo(profile, ADDRESS_HOME_DEPENDENT_LOCALITY));
  s->BindString16(index++, GetInfo(profile, ADDRESS_HOME_CITY));
  s->BindString16(index++, GetInfo(profile, ADDRESS_HOME_STATE));
  s->BindString16(index++, GetInfo(profile, ADDRESS_HOME_ZIP));
  s->BindString16(index++, GetInfo(profile, ADDRESS_HOME_SORTING_CODE));
  s->BindString16(index++, GetInfo(profile, ADDRESS_HOME_COUNTRY));
  s->BindInt64(index++, profile.use_count());
  s->BindInt64(index++, profile.use_date().ToTimeT());
  s->BindInt64(index++, modification_date.ToTimeT());
  s->BindString(index++, profile.origin());
  s->BindString(index++, profile.language_code());
  s->BindInt64(index++, profile.GetClientValidityBitfieldValue());
  s->BindBool(index++, profile.is_client_validity_states_updated());
}

void AddAutofillProfileDetailsFromStatement(const sql::Statement& s,
                                            AutofillProfile* profile) {
  int index = 1;  // 0 is for the guid.
  profile->SetRawInfo(COMPANY_NAME, s.ColumnString16(index++));
  profile->SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, s.ColumnString16(index++));
  profile->SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                      s.ColumnString16(index++));
  profile->SetRawInfo(ADDRESS_HOME_CITY, s.ColumnString16(index++));
  profile->SetRawInfo(ADDRESS_HOME_STATE, s.ColumnString16(index++));
  profile->SetRawInfo(ADDRESS_HOME_ZIP, s.ColumnString16(index++));
  profile->SetRawInfo(ADDRESS_HOME_SORTING_CODE, s.ColumnString16(index++));
  profile->SetRawInfo(ADDRESS_HOME_COUNTRY, s.ColumnString16(index++));
  profile->set_use_count(s.ColumnInt64(index++));
  profile->set_use_date(base::Time::FromTimeT(s.ColumnInt64(index++)));
  profile->set_modification_date(base::Time::FromTimeT(s.ColumnInt64(index++)));
  profile->set_origin(s.ColumnString(index++));
  profile->set_language_code(s.ColumnString(index++));
  profile->SetClientValidityFromBitfieldValue(s.ColumnInt64(index++));
  profile->set_is_client_validity_states_updated(s.ColumnBool(index++));
}

void BindEncryptedCardToColumn(sql::Statement* s,
                               int column_index,
                               const base::string16& number,
                               const AutofillTableEncryptor& encryptor) {
  std::string encrypted_data;
  encryptor.EncryptString16(number, &encrypted_data);
  s->BindBlob(column_index, encrypted_data.data(),
              static_cast<int>(encrypted_data.length()));
}

void BindCreditCardToStatement(const CreditCard& credit_card,
                               const base::Time& modification_date,
                               sql::Statement* s,
                               const AutofillTableEncryptor& encryptor) {
  DCHECK(base::IsValidGUID(credit_card.guid()));
  int index = 0;
  s->BindString(index++, credit_card.guid());

  s->BindString16(index++, GetInfo(credit_card, CREDIT_CARD_NAME_FULL));
  s->BindString16(index++, GetInfo(credit_card, CREDIT_CARD_EXP_MONTH));
  s->BindString16(index++, GetInfo(credit_card, CREDIT_CARD_EXP_4_DIGIT_YEAR));
  BindEncryptedCardToColumn(
      s, index++, credit_card.GetRawInfo(CREDIT_CARD_NUMBER), encryptor);

  s->BindInt64(index++, credit_card.use_count());
  s->BindInt64(index++, credit_card.use_date().ToTimeT());
  s->BindInt64(index++, modification_date.ToTimeT());
  s->BindString(index++, credit_card.origin());
  s->BindString(index++, credit_card.billing_address_id());
}

base::string16 UnencryptedCardFromColumn(
    const sql::Statement& s,
    int column_index,
    const AutofillTableEncryptor& encryptor) {
  base::string16 credit_card_number;
  int encrypted_number_len = s.ColumnByteLength(column_index);
  if (encrypted_number_len) {
    std::string encrypted_number;
    encrypted_number.resize(encrypted_number_len);
    memcpy(&encrypted_number[0], s.ColumnBlob(column_index),
           encrypted_number_len);
    encryptor.DecryptString16(encrypted_number, &credit_card_number);
  }
  return credit_card_number;
}

std::unique_ptr<CreditCard> CreditCardFromStatement(
    const sql::Statement& s,
    const AutofillTableEncryptor& encryptor) {
  std::unique_ptr<CreditCard> credit_card(new CreditCard);

  int index = 0;
  credit_card->set_guid(s.ColumnString(index++));
  DCHECK(base::IsValidGUID(credit_card->guid()));

  credit_card->SetRawInfo(CREDIT_CARD_NAME_FULL, s.ColumnString16(index++));
  credit_card->SetRawInfo(CREDIT_CARD_EXP_MONTH, s.ColumnString16(index++));
  credit_card->SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR,
                          s.ColumnString16(index++));
  credit_card->SetRawInfo(CREDIT_CARD_NUMBER,
                          UnencryptedCardFromColumn(s, index++, encryptor));
  credit_card->set_use_count(s.ColumnInt64(index++));
  credit_card->set_use_date(base::Time::FromTimeT(s.ColumnInt64(index++)));
  credit_card->set_modification_date(
      base::Time::FromTimeT(s.ColumnInt64(index++)));
  credit_card->set_origin(s.ColumnString(index++));
  credit_card->set_billing_address_id(s.ColumnString(index++));
  credit_card->set_bank_name(s.ColumnString(index++));

  return credit_card;
}

bool AddAutofillProfileNamesToProfile(sql::Database* db,
                                      AutofillProfile* profile) {
  // TODO(estade): update schema so that multiple names are not associated per
  // unique profile guid. Please refer https://crbug.com/497934.
  sql::Statement s(db->GetUniqueStatement(
      "SELECT guid, first_name, middle_name, last_name, full_name "
      "FROM autofill_profile_names "
      "WHERE guid=?"
      "LIMIT 1"));
  s.BindString(0, profile->guid());

  if (!s.is_valid())
    return false;

  if (s.Step()) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));
    profile->SetRawInfo(NAME_FIRST, s.ColumnString16(1));
    profile->SetRawInfo(NAME_MIDDLE, s.ColumnString16(2));
    profile->SetRawInfo(NAME_LAST, s.ColumnString16(3));
    profile->SetRawInfo(NAME_FULL, s.ColumnString16(4));
  }
  return s.Succeeded();
}

bool AddAutofillProfileEmailsToProfile(sql::Database* db,
                                       AutofillProfile* profile) {
  // TODO(estade): update schema so that multiple emails are not associated per
  // unique profile guid. Please refer https://crbug.com/497934.
  sql::Statement s(db->GetUniqueStatement(
      "SELECT guid, email FROM autofill_profile_emails WHERE guid=? LIMIT 1"));
  s.BindString(0, profile->guid());

  if (!s.is_valid())
    return false;

  if (s.Step()) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));
    profile->SetRawInfo(EMAIL_ADDRESS, s.ColumnString16(1));
  }
  return s.Succeeded();
}

bool AddAutofillProfilePhonesToProfile(sql::Database* db,
                                       AutofillProfile* profile) {
  // TODO(estade): update schema so that multiple phone numbers are not
  // associated per unique profile guid. Please refer https://crbug.com/497934.
  sql::Statement s(db->GetUniqueStatement(
      "SELECT guid, number FROM autofill_profile_phones WHERE guid=? LIMIT 1"));
  s.BindString(0, profile->guid());

  if (!s.is_valid())
    return false;

  if (s.Step()) {
    DCHECK_EQ(profile->guid(), s.ColumnString(0));
    profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, s.ColumnString16(1));
  }
  return s.Succeeded();
}

bool AddAutofillProfileNames(const AutofillProfile& profile,
                             sql::Database* db) {
  // Add the new name.
  sql::Statement s(db->GetUniqueStatement(
      "INSERT INTO autofill_profile_names"
      " (guid, first_name, middle_name, last_name, full_name) "
      "VALUES (?,?,?,?,?)"));
  s.BindString(0, profile.guid());
  s.BindString16(1, profile.GetRawInfo(NAME_FIRST));
  s.BindString16(2, profile.GetRawInfo(NAME_MIDDLE));
  s.BindString16(3, profile.GetRawInfo(NAME_LAST));
  s.BindString16(4, profile.GetRawInfo(NAME_FULL));

  return s.Run();
}

bool AddAutofillProfileEmails(const AutofillProfile& profile,
                              sql::Database* db) {
  // Add the new email.
  sql::Statement s(db->GetUniqueStatement(
      "INSERT INTO autofill_profile_emails (guid, email) VALUES (?,?)"));
  s.BindString(0, profile.guid());
  s.BindString16(1, profile.GetRawInfo(EMAIL_ADDRESS));

  return s.Run();
}

bool AddAutofillProfilePhones(const AutofillProfile& profile,
                              sql::Database* db) {
  // Add the new number.
  sql::Statement s(db->GetUniqueStatement(
      "INSERT INTO autofill_profile_phones (guid, number) VALUES (?,?)"));
  s.BindString(0, profile.guid());
  s.BindString16(1, profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));

  return s.Run();
}

bool AddAutofillProfilePieces(const AutofillProfile& profile,
                              sql::Database* db) {
  if (!AddAutofillProfileNames(profile, db))
    return false;

  if (!AddAutofillProfileEmails(profile, db))
    return false;

  if (!AddAutofillProfilePhones(profile, db))
    return false;

  return true;
}

bool RemoveAutofillProfilePieces(const std::string& guid, sql::Database* db) {
  sql::Statement s1(db->GetUniqueStatement(
      "DELETE FROM autofill_profile_names WHERE guid = ?"));
  s1.BindString(0, guid);

  if (!s1.Run())
    return false;

  sql::Statement s2(db->GetUniqueStatement(
      "DELETE FROM autofill_profile_emails WHERE guid = ?"));
  s2.BindString(0, guid);

  if (!s2.Run())
    return false;

  sql::Statement s3(db->GetUniqueStatement(
      "DELETE FROM autofill_profile_phones WHERE guid = ?"));
  s3.BindString(0, guid);

  return s3.Run();
}

WebDatabaseTable::TypeKey GetKey() {
  // We just need a unique constant. Use the address of a static that
  // COMDAT folding won't touch in an optimizing linker.
  static int table_key = 0;
  return reinterpret_cast<void*>(&table_key);
}

time_t GetEndTime(const base::Time& end) {
  if (end.is_null() || end == base::Time::Max())
    return std::numeric_limits<time_t>::max();

  return end.ToTimeT();
}

std::string ServerStatusEnumToString(CreditCard::ServerStatus status) {
  switch (status) {
    case CreditCard::EXPIRED:
      return "EXPIRED";

    case CreditCard::OK:
      return "OK";
  }

  NOTREACHED();
  return "OK";
}

CreditCard::ServerStatus ServerStatusStringToEnum(const std::string& status) {
  if (status == "EXPIRED")
    return CreditCard::EXPIRED;

  DCHECK_EQ("OK", status);
  return CreditCard::OK;
}

// Returns |s| with |escaper| in front of each of occurrence of a character from
// |special_chars|. Any occurrence of |escaper| in |s| is doubled. For example,
// Substitute("hello_world!", "_%", '!'') returns "hello!_world!!".
base::string16 Substitute(const base::string16& s,
                          const base::string16& special_chars,
                          const base::char16& escaper) {
  // Prepend |escaper| to the list of |special_chars|.
  base::string16 escape_wildcards(special_chars);
  escape_wildcards.insert(escape_wildcards.begin(), escaper);

  // Prepend the |escaper| just before |special_chars| in |s|.
  base::string16 result(s);
  for (base::char16 c : escape_wildcards) {
    for (size_t pos = 0; (pos = result.find(c, pos)) != base::string16::npos;
         pos += 2) {
      result.insert(result.begin() + pos, escaper);
    }
  }

  return result;
}

}  // namespace

// static
const size_t AutofillTable::kMaxDataLength = 1024;

AutofillTable::AutofillTable()
    : autofill_table_encryptor_(
          AutofillTableEncryptorFactory::GetInstance()->Create()) {
  DCHECK(autofill_table_encryptor_);
}

AutofillTable::~AutofillTable() {}

AutofillTable* AutofillTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<AutofillTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey AutofillTable::GetTypeKey() const {
  return GetKey();
}

bool AutofillTable::CreateTablesIfNecessary() {
  return (InitMainTable() && InitCreditCardsTable() && InitProfilesTable() &&
          InitProfileNamesTable() && InitProfileEmailsTable() &&
          InitProfilePhonesTable() && InitProfileTrashTable() &&
          InitMaskedCreditCardsTable() && InitUnmaskedCreditCardsTable() &&
          InitServerCardMetadataTable() && InitServerAddressesTable() &&
          InitServerAddressMetadataTable() && InitAutofillSyncMetadataTable() &&
          InitModelTypeStateTable() && InitPaymentsCustomerDataTable() &&
          InitPaymentsUPIVPATable());
}

bool AutofillTable::IsSyncable() {
  return true;
}

bool AutofillTable::MigrateToVersion(int version,
                                     bool* update_compatible_version) {
  // Migrate if necessary.
  switch (version) {
    case 54:
      *update_compatible_version = true;
      return MigrateToVersion54AddI18nFieldsAndRemoveDeprecatedFields();
    case 55:
      *update_compatible_version = true;
      return MigrateToVersion55MergeAutofillDatesTable();
    case 56:
      *update_compatible_version = true;
      return MigrateToVersion56AddProfileLanguageCodeForFormatting();
    case 57:
      *update_compatible_version = true;
      return MigrateToVersion57AddFullNameField();
    case 60:
      *update_compatible_version = false;
      return MigrateToVersion60AddServerCards();
    case 61:
      *update_compatible_version = false;
      return MigrateToVersion61AddUsageStats();
    case 62:
      *update_compatible_version = false;
      return MigrateToVersion62AddUsageStatsForUnmaskedCards();
    case 63:
      *update_compatible_version = false;
      return MigrateToVersion63AddServerRecipientName();
    case 64:
      *update_compatible_version = false;
      return MigrateToVersion64AddUnmaskDate();
    case 65:
      *update_compatible_version = false;
      return MigrateToVersion65AddServerMetadataTables();
    case 66:
      *update_compatible_version = false;
      return MigrateToVersion66AddCardBillingAddress();
    case 67:
      *update_compatible_version = false;
      return MigrateToVersion67AddMaskedCardBillingAddress();
    case 70:
      *update_compatible_version = false;
      return MigrateToVersion70AddSyncMetadata();
    case 71:
      *update_compatible_version = true;
      return MigrateToVersion71AddHasConvertedAndBillingAddressIdMetadata();
    case 72:
      *update_compatible_version = true;
      return MigrateToVersion72RenameCardTypeToIssuerNetwork();
    case 73:
      *update_compatible_version = false;
      return MigrateToVersion73AddMaskedCardBankName();
    case 74:
      *update_compatible_version = false;
      return MigrateToVersion74AddServerCardTypeColumn();
    case 75:
      *update_compatible_version = false;
      return MigrateToVersion75AddProfileValidityBitfieldColumn();
    case 78:
      *update_compatible_version = true;
      return MigrateToVersion78AddModelTypeColumns();
    case 80:
      *update_compatible_version = true;
      return MigrateToVersion80AddIsClientValidityStatesUpdatedColumn();
    case 81:
      *update_compatible_version = true;
      return MigrateToVersion81CleanUpWrongModelTypeData();
  }
  return true;
}

bool AutofillTable::AddFormFieldValues(
    const std::vector<FormFieldData>& elements,
    std::vector<AutofillChange>* changes) {
  return AddFormFieldValuesTime(elements, changes, AutofillClock::Now());
}

bool AutofillTable::AddFormFieldValue(const FormFieldData& element,
                                      std::vector<AutofillChange>* changes) {
  return AddFormFieldValueTime(element, changes, AutofillClock::Now());
}

bool AutofillTable::GetFormValuesForElementName(
    const base::string16& name,
    const base::string16& prefix,
    std::vector<AutofillEntry>* entries,
    int limit) {
  DCHECK(entries);
  bool succeeded = false;

  if (prefix.empty()) {
    sql::Statement s;
    s.Assign(db_->GetUniqueStatement(
        "SELECT name, value, date_created, date_last_used FROM autofill "
        "WHERE name = ? "
        "ORDER BY count DESC LIMIT ?"));
    s.BindString16(0, name);
    s.BindInt(1, limit);

    entries->clear();
    while (s.Step()) {
      entries->push_back(AutofillEntry(
          AutofillKey(/*name=*/s.ColumnString16(0),
                      /*value=*/s.ColumnString16(1)),
          /*date_created=*/base::Time::FromTimeT(s.ColumnInt64(2)),
          /*date_last_used=*/base::Time::FromTimeT(s.ColumnInt64(3))));
    }

    succeeded = s.Succeeded();
  } else {
    base::string16 prefix_lower = base::i18n::ToLower(prefix);
    base::string16 next_prefix = prefix_lower;
    next_prefix.back()++;

    sql::Statement s1;
    s1.Assign(db_->GetUniqueStatement(
        "SELECT name, value, date_created, date_last_used FROM autofill "
        "WHERE name = ? AND "
        "value_lower >= ? AND "
        "value_lower < ? "
        "ORDER BY count DESC "
        "LIMIT ?"));
    s1.BindString16(0, name);
    s1.BindString16(1, prefix_lower);
    s1.BindString16(2, next_prefix);
    s1.BindInt(3, limit);

    entries->clear();
    while (s1.Step()) {
      entries->push_back(AutofillEntry(
          AutofillKey(/*name=*/s1.ColumnString16(0),
                      /*value=*/s1.ColumnString16(1)),
          /*date_created=*/base::Time::FromTimeT(s1.ColumnInt64(2)),
          /*date_last_used=*/base::Time::FromTimeT(s1.ColumnInt64(3))));
    }

    succeeded = s1.Succeeded();

    if (IsFeatureSubstringMatchEnabled()) {
      sql::Statement s2;
      s2.Assign(db_->GetUniqueStatement(
          "SELECT name, value, date_created, date_last_used FROM autofill "
          "WHERE name = ? AND ("
          " value LIKE '% ' || :prefix || '%' ESCAPE '!' OR "
          " value LIKE '%.' || :prefix || '%' ESCAPE '!' OR "
          " value LIKE '%,' || :prefix || '%' ESCAPE '!' OR "
          " value LIKE '%-' || :prefix || '%' ESCAPE '!' OR "
          " value LIKE '%@' || :prefix || '%' ESCAPE '!' OR "
          " value LIKE '%!_' || :prefix || '%' ESCAPE '!' ) "
          "ORDER BY count DESC "
          "LIMIT ?"));

      s2.BindString16(0, name);
      // escaper as L'!' -> 0x21.
      s2.BindString16(1,
                      Substitute(prefix_lower, base::ASCIIToUTF16("_%"), 0x21));
      s2.BindInt(2, limit);
      while (s2.Step()) {
        entries->push_back(AutofillEntry(
            AutofillKey(/*name=*/s2.ColumnString16(0),
                        /*value=*/s2.ColumnString16(1)),
            /*date_created=*/base::Time::FromTimeT(s2.ColumnInt64(2)),
            /*date_last_used=*/base::Time::FromTimeT(s2.ColumnInt64(3))));
      }

      succeeded &= s2.Succeeded();
    }
  }

  return succeeded;
}

bool AutofillTable::RemoveFormElementsAddedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    std::vector<AutofillChange>* changes) {
  const time_t delete_begin_time_t = delete_begin.ToTimeT();
  const time_t delete_end_time_t = GetEndTime(delete_end);

  // Query for the name, value, count, and access dates of all form elements
  // that were used between the given times.
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT name, value, count, date_created, date_last_used FROM autofill "
      "WHERE (date_created >= ? AND date_created < ?) OR "
      "      (date_last_used >= ? AND date_last_used < ?)"));
  s.BindInt64(0, delete_begin_time_t);
  s.BindInt64(1, delete_end_time_t);
  s.BindInt64(2, delete_begin_time_t);
  s.BindInt64(3, delete_end_time_t);

  std::vector<AutofillUpdate> updates;
  std::vector<AutofillChange> tentative_changes;
  while (s.Step()) {
    base::string16 name = s.ColumnString16(0);
    base::string16 value = s.ColumnString16(1);
    int count = s.ColumnInt(2);
    time_t date_created_time_t = s.ColumnInt64(3);
    time_t date_last_used_time_t = s.ColumnInt64(4);

    // If *all* uses of the element were between |delete_begin| and
    // |delete_end|, then delete the element.  Otherwise, update the use
    // timestamps and use count.
    AutofillChange::Type change_type;
    if (date_created_time_t >= delete_begin_time_t &&
        date_last_used_time_t < delete_end_time_t) {
      change_type = AutofillChange::REMOVE;
    } else {
      change_type = AutofillChange::UPDATE;

      // For all updated elements, set either date_created or date_last_used so
      // that the range [date_created, date_last_used] no longer overlaps with
      // [delete_begin, delete_end). Update the count by interpolating.
      // Precisely, compute the average amount of time between increments to the
      // count in the original range [date_created, date_last_used]:
      //   avg_delta = (date_last_used_orig - date_created_orig) / (count - 1)
      // The count can be expressed as
      //   count = 1 + (date_last_used - date_created) / avg_delta
      // Hence, update the count to
      //   count_new = 1 + (date_last_used_new - date_created_new) / avg_delta
      //             = 1 + ((count - 1) *
      //                    (date_last_used_new - date_created_new) /
      //                    (date_last_used_orig - date_created_orig))
      // Interpolating might not give a result that completely accurately
      // reflects the user's history, but it's the best that can be done given
      // the information in the database.
      AutofillUpdate updated_entry;
      updated_entry.name = name;
      updated_entry.value = value;
      updated_entry.date_created = date_created_time_t < delete_begin_time_t
                                       ? date_created_time_t
                                       : delete_end_time_t;
      updated_entry.date_last_used = date_last_used_time_t >= delete_end_time_t
                                         ? date_last_used_time_t
                                         : delete_begin_time_t - 1;
      updated_entry.count =
          1 + gfx::ToRoundedInt(
                  1.0 * (count - 1) *
                  (updated_entry.date_last_used - updated_entry.date_created) /
                  (date_last_used_time_t - date_created_time_t));
      updates.push_back(updated_entry);
    }

    tentative_changes.push_back(
        AutofillChange(change_type, AutofillKey(name, value)));
  }
  if (!s.Succeeded())
    return false;

  // As a single transaction, remove or update the elements appropriately.
  sql::Statement s_delete(db_->GetUniqueStatement(
      "DELETE FROM autofill WHERE date_created >= ? AND date_last_used < ?"));
  s_delete.BindInt64(0, delete_begin_time_t);
  s_delete.BindInt64(1, delete_end_time_t);
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;
  if (!s_delete.Run())
    return false;
  for (size_t i = 0; i < updates.size(); ++i) {
    sql::Statement s_update(db_->GetUniqueStatement(
        "UPDATE autofill SET date_created = ?, date_last_used = ?, count = ?"
        "WHERE name = ? AND value = ?"));
    s_update.BindInt64(0, updates[i].date_created);
    s_update.BindInt64(1, updates[i].date_last_used);
    s_update.BindInt(2, updates[i].count);
    s_update.BindString16(3, updates[i].name);
    s_update.BindString16(4, updates[i].value);
    if (!s_update.Run())
      return false;
  }
  if (!transaction.Commit())
    return false;

  *changes = tentative_changes;
  return true;
}

bool AutofillTable::RemoveExpiredFormElements(
    std::vector<AutofillChange>* changes) {
  const int64_t period = kAutocompleteRetentionPolicyPeriodInDays;
  const auto change_type = AutofillChange::EXPIRE;

  base::Time expiration_time =
      AutofillClock::Now() - base::TimeDelta::FromDays(period);

  // Query for the name and value of all form elements that were last used
  // before the |expiration_time|.
  sql::Statement select_for_delete(db_->GetUniqueStatement(
      "SELECT name, value FROM autofill WHERE date_last_used < ?"));
  select_for_delete.BindInt64(0, expiration_time.ToTimeT());
  std::vector<AutofillChange> tentative_changes;
  while (select_for_delete.Step()) {
    base::string16 name = select_for_delete.ColumnString16(0);
    base::string16 value = select_for_delete.ColumnString16(1);
    tentative_changes.push_back(
        AutofillChange(change_type, AutofillKey(name, value)));
  }

  if (!select_for_delete.Succeeded())
    return false;

  sql::Statement delete_data_statement(
      db_->GetUniqueStatement("DELETE FROM autofill WHERE date_last_used < ?"));
  delete_data_statement.BindInt64(0, expiration_time.ToTimeT());
  if (!delete_data_statement.Run())
    return false;

  *changes = tentative_changes;
  return true;
}

bool AutofillTable::RemoveFormElement(const base::string16& name,
                                      const base::string16& value) {
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM autofill WHERE name = ? AND value= ?"));
  s.BindString16(0, name);
  s.BindString16(1, value);
  return s.Run();
}

int AutofillTable::GetCountOfValuesContainedBetween(const base::Time& begin,
                                                    const base::Time& end) {
  const time_t begin_time_t = begin.ToTimeT();
  const time_t end_time_t = GetEndTime(end);

  sql::Statement s(db_->GetUniqueStatement(
      "SELECT COUNT(DISTINCT(value1)) FROM ( "
      "  SELECT value AS value1 FROM autofill "
      "  WHERE NOT EXISTS ( "
      "    SELECT value AS value2, date_created, date_last_used FROM autofill "
      "    WHERE value1 = value2 AND "
      "          (date_created < ? OR date_last_used >= ?)))"));
  s.BindInt64(0, begin_time_t);
  s.BindInt64(1, end_time_t);

  if (!s.Step()) {
    NOTREACHED();
    return false;
  }
  return s.ColumnInt(0);
}

bool AutofillTable::GetAllAutofillEntries(std::vector<AutofillEntry>* entries) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT name, value, date_created, date_last_used FROM autofill"));

  while (s.Step()) {
    base::string16 name = s.ColumnString16(0);
    base::string16 value = s.ColumnString16(1);
    base::Time date_created = base::Time::FromTimeT(s.ColumnInt64(2));
    base::Time date_last_used = base::Time::FromTimeT(s.ColumnInt64(3));
    entries->push_back(
        AutofillEntry(AutofillKey(name, value), date_created, date_last_used));
  }

  return s.Succeeded();
}

bool AutofillTable::GetAutofillTimestamps(const base::string16& name,
                                          const base::string16& value,
                                          base::Time* date_created,
                                          base::Time* date_last_used) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT date_created, date_last_used FROM autofill "
      "WHERE name = ? AND value = ?"));
  s.BindString16(0, name);
  s.BindString16(1, value);
  if (!s.Step())
    return false;

  *date_created = base::Time::FromTimeT(s.ColumnInt64(0));
  *date_last_used = base::Time::FromTimeT(s.ColumnInt64(1));

  DCHECK(!s.Step());
  return true;
}

bool AutofillTable::UpdateAutofillEntries(
    const std::vector<AutofillEntry>& entries) {
  if (entries.empty())
    return true;

  // Remove all existing entries.
  for (size_t i = 0; i < entries.size(); ++i) {
    sql::Statement s(db_->GetUniqueStatement(
        "DELETE FROM autofill WHERE name = ? AND value = ?"));
    s.BindString16(0, entries[i].key().name());
    s.BindString16(1, entries[i].key().value());
    if (!s.Run())
      return false;
  }

  // Insert all the supplied autofill entries.
  for (size_t i = 0; i < entries.size(); ++i) {
    if (!InsertAutofillEntry(entries[i]))
      return false;
  }

  return true;
}

bool AutofillTable::AddAutofillProfile(const AutofillProfile& profile) {
  if (IsAutofillGUIDInTrash(profile.guid()))
    return true;

  sql::Statement s(db_->GetUniqueStatement(
      "INSERT INTO autofill_profiles"
      "(guid, company_name, street_address, dependent_locality, city, state,"
      " zipcode, sorting_code, country_code, use_count, use_date, "
      " date_modified, origin, language_code, validity_bitfield, "
      " is_client_validity_states_updated)"
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  BindAutofillProfileToStatement(profile, AutofillClock::Now(), &s);

  if (!s.Run())
    return false;

  return AddAutofillProfilePieces(profile, db_);
}

bool AutofillTable::UpdateAutofillProfile(const AutofillProfile& profile) {
  DCHECK(base::IsValidGUID(profile.guid()));

  // Don't update anything until the trash has been emptied.  There may be
  // pending modifications to process.
  if (!IsAutofillProfilesTrashEmpty())
    return true;

  std::unique_ptr<AutofillProfile> old_profile =
      GetAutofillProfile(profile.guid());
  if (!old_profile)
    return false;

  bool update_modification_date = *old_profile != profile;

  sql::Statement s(db_->GetUniqueStatement(
      "UPDATE autofill_profiles "
      "SET guid=?, company_name=?, street_address=?, dependent_locality=?, "
      "    city=?, state=?, zipcode=?, sorting_code=?, country_code=?, "
      "    use_count=?, use_date=?, date_modified=?, origin=?, "
      "    language_code=?, validity_bitfield=?, "
      "    is_client_validity_states_updated=?"
      "WHERE guid=?"));
  BindAutofillProfileToStatement(profile,
                                 update_modification_date
                                     ? AutofillClock::Now()
                                     : old_profile->modification_date(),
                                 &s);
  s.BindString(16, profile.guid());

  bool result = s.Run();
  DCHECK_GT(db_->GetLastChangeCount(), 0);
  if (!result)
    return result;

  // Remove the old names, emails, and phone numbers.
  if (!RemoveAutofillProfilePieces(profile.guid(), db_))
    return false;

  return AddAutofillProfilePieces(profile, db_);
}

bool AutofillTable::RemoveAutofillProfile(const std::string& guid) {
  DCHECK(base::IsValidGUID(guid));

  if (IsAutofillGUIDInTrash(guid)) {
    sql::Statement s_trash(db_->GetUniqueStatement(
        "DELETE FROM autofill_profiles_trash WHERE guid = ?"));
    s_trash.BindString(0, guid);

    bool success = s_trash.Run();
    DCHECK_GT(db_->GetLastChangeCount(), 0) << "Expected item in trash";
    return success;
  }

  sql::Statement s(
      db_->GetUniqueStatement("DELETE FROM autofill_profiles WHERE guid = ?"));
  s.BindString(0, guid);

  if (!s.Run())
    return false;

  return RemoveAutofillProfilePieces(guid, db_);
}

std::unique_ptr<AutofillProfile> AutofillTable::GetAutofillProfile(
    const std::string& guid) {
  DCHECK(base::IsValidGUID(guid));
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid, company_name, street_address, dependent_locality, city,"
      " state, zipcode, sorting_code, country_code, use_count, use_date,"
      " date_modified, origin, language_code, validity_bitfield,"
      " is_client_validity_states_updated "
      "FROM autofill_profiles "
      "WHERE guid=?"));
  s.BindString(0, guid);

  if (!s.Step())
    return nullptr;

  std::unique_ptr<AutofillProfile> profile(new AutofillProfile);
  profile->set_guid(s.ColumnString(0));
  DCHECK(base::IsValidGUID(profile->guid()));

  // Get associated name info using guid.
  AddAutofillProfileNamesToProfile(db_, profile.get());

  // Get associated email info using guid.
  AddAutofillProfileEmailsToProfile(db_, profile.get());

  // Get associated phone info using guid.
  AddAutofillProfilePhonesToProfile(db_, profile.get());

  // The details should be added after the other info to make sure they don't
  // change when we change the names/emails/phones.
  AddAutofillProfileDetailsFromStatement(s, profile.get());

  return profile;
}

bool AutofillTable::GetAutofillProfiles(
    std::vector<std::unique_ptr<AutofillProfile>>* profiles) {
  DCHECK(profiles);
  profiles->clear();

  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid FROM autofill_profiles ORDER BY date_modified DESC, guid"));

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    std::unique_ptr<AutofillProfile> profile = GetAutofillProfile(guid);
    if (!profile)
      return false;
    profiles->push_back(std::move(profile));
  }

  return s.Succeeded();
}

bool AutofillTable::GetServerProfiles(
    std::vector<std::unique_ptr<AutofillProfile>>* profiles) const {
  profiles->clear();

  sql::Statement s(db_->GetUniqueStatement(
      "SELECT "
      "id,"
      "use_count,"
      "use_date,"
      "recipient_name,"
      "company_name,"
      "street_address,"
      "address_1,"     // ADDRESS_HOME_STATE
      "address_2,"     // ADDRESS_HOME_CITY
      "address_3,"     // ADDRESS_HOME_DEPENDENT_LOCALITY
      "address_4,"     // Not supported in AutofillProfile yet.
      "postal_code,"   // ADDRESS_HOME_ZIP
      "sorting_code,"  // ADDRESS_HOME_SORTING_CODE
      "country_code,"  // ADDRESS_HOME_COUNTRY
      "phone_number,"  // PHONE_HOME_WHOLE_NUMBER
      "language_code, "
      "has_converted "
      "FROM server_addresses addresses "
      "LEFT OUTER JOIN server_address_metadata USING (id)"));

  while (s.Step()) {
    int index = 0;
    std::unique_ptr<AutofillProfile> profile =
        std::make_unique<AutofillProfile>(AutofillProfile::SERVER_PROFILE,
                                          s.ColumnString(index++));
    profile->set_use_count(s.ColumnInt64(index++));
    profile->set_use_date(
        base::Time::FromInternalValue(s.ColumnInt64(index++)));
    // Modification date is not tracked for server profiles. Explicitly set it
    // here to override the default value of AutofillClock::Now().
    profile->set_modification_date(base::Time());

    base::string16 recipient_name = s.ColumnString16(index++);
    profile->SetRawInfo(COMPANY_NAME, s.ColumnString16(index++));
    profile->SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, s.ColumnString16(index++));
    profile->SetRawInfo(ADDRESS_HOME_STATE, s.ColumnString16(index++));
    profile->SetRawInfo(ADDRESS_HOME_CITY, s.ColumnString16(index++));
    profile->SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                        s.ColumnString16(index++));
    index++;  // Skip address_4 which we haven't added to AutofillProfile yet.
    profile->SetRawInfo(ADDRESS_HOME_ZIP, s.ColumnString16(index++));
    profile->SetRawInfo(ADDRESS_HOME_SORTING_CODE, s.ColumnString16(index++));
    profile->SetRawInfo(ADDRESS_HOME_COUNTRY, s.ColumnString16(index++));
    base::string16 phone_number = s.ColumnString16(index++);
    profile->set_language_code(s.ColumnString(index++));
    profile->set_has_converted(s.ColumnBool(index++));

    // SetInfo instead of SetRawInfo so the constituent pieces will be parsed
    // for these data types.
    profile->SetInfo(NAME_FULL, recipient_name, profile->language_code());
    profile->SetInfo(PHONE_HOME_WHOLE_NUMBER, phone_number,
                     profile->language_code());

    profiles->push_back(std::move(profile));
  }

  return s.Succeeded();
}

void AutofillTable::SetServerProfiles(
    const std::vector<AutofillProfile>& profiles) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return;

  // Delete all old ones first.
  sql::Statement delete_old(
      db_->GetUniqueStatement("DELETE FROM server_addresses"));
  delete_old.Run();

  sql::Statement insert(db_->GetUniqueStatement(
      "INSERT INTO server_addresses("
      "id,"
      "recipient_name,"
      "company_name,"
      "street_address,"
      "address_1,"     // ADDRESS_HOME_STATE
      "address_2,"     // ADDRESS_HOME_CITY
      "address_3,"     // ADDRESS_HOME_DEPENDENT_LOCALITY
      "address_4,"     // Not supported in AutofillProfile yet.
      "postal_code,"   // ADDRESS_HOME_ZIP
      "sorting_code,"  // ADDRESS_HOME_SORTING_CODE
      "country_code,"  // ADDRESS_HOME_COUNTRY
      "phone_number,"  // PHONE_HOME_WHOLE_NUMBER
      "language_code) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  for (const auto& profile : profiles) {
    DCHECK(profile.record_type() == AutofillProfile::SERVER_PROFILE);

    int index = 0;
    insert.BindString(index++, profile.server_id());
    insert.BindString16(index++, profile.GetRawInfo(NAME_FULL));
    insert.BindString16(index++, profile.GetRawInfo(COMPANY_NAME));
    insert.BindString16(index++,
                        profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
    insert.BindString16(index++, profile.GetRawInfo(ADDRESS_HOME_STATE));
    insert.BindString16(index++, profile.GetRawInfo(ADDRESS_HOME_CITY));
    insert.BindString16(index++,
                        profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY));
    index++;  // SKip address_4 which we haven't added to AutofillProfile yet.
    insert.BindString16(index++, profile.GetRawInfo(ADDRESS_HOME_ZIP));
    insert.BindString16(index++, profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE));
    insert.BindString16(index++, profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
    insert.BindString16(index++, profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
    insert.BindString(index++, profile.language_code());

    insert.Run();
    insert.Reset(true);

    // Save the use count and use date of the profile.
    UpdateServerAddressMetadata(profile);
  }

  // Delete metadata that's no longer relevant.
  sql::Statement metadata_delete(db_->GetUniqueStatement(
      "DELETE FROM server_address_metadata WHERE id NOT IN "
      "(SELECT id FROM server_addresses)"));
  metadata_delete.Run();

  transaction.Commit();
}

bool AutofillTable::AddCreditCard(const CreditCard& credit_card) {
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT INTO credit_cards"
      "(guid, name_on_card, expiration_month, expiration_year, "
      " card_number_encrypted, use_count, use_date, date_modified, origin,"
      " billing_address_id)"
      "VALUES (?,?,?,?,?,?,?,?,?,?)"));
  BindCreditCardToStatement(credit_card, AutofillClock::Now(), &s,
                            *autofill_table_encryptor_);

  if (!s.Run())
    return false;

  DCHECK_GT(db_->GetLastChangeCount(), 0);
  return true;
}

bool AutofillTable::UpdateCreditCard(const CreditCard& credit_card) {
  DCHECK(base::IsValidGUID(credit_card.guid()));

  std::unique_ptr<CreditCard> old_credit_card =
      GetCreditCard(credit_card.guid());
  if (!old_credit_card)
    return false;

  bool update_modification_date = *old_credit_card != credit_card;

  sql::Statement s(db_->GetUniqueStatement(
      "UPDATE credit_cards "
      "SET guid=?, name_on_card=?, expiration_month=?,"
      "expiration_year=?, card_number_encrypted=?, use_count=?, use_date=?,"
      "date_modified=?, origin=?, billing_address_id=?"
      "WHERE guid=?1"));
  BindCreditCardToStatement(credit_card,
                            update_modification_date
                                ? AutofillClock::Now()
                                : old_credit_card->modification_date(),
                            &s, *autofill_table_encryptor_);

  bool result = s.Run();
  DCHECK_GT(db_->GetLastChangeCount(), 0);
  return result;
}

bool AutofillTable::RemoveCreditCard(const std::string& guid) {
  DCHECK(base::IsValidGUID(guid));
  sql::Statement s(
      db_->GetUniqueStatement("DELETE FROM credit_cards WHERE guid = ?"));
  s.BindString(0, guid);

  return s.Run();
}

bool AutofillTable::AddFullServerCreditCard(const CreditCard& credit_card) {
  DCHECK_EQ(CreditCard::FULL_SERVER_CARD, credit_card.record_type());
  DCHECK(!credit_card.number().empty());
  DCHECK(!credit_card.server_id().empty());

  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  // Make sure there aren't duplicates for this card.
  DeleteFromUnmaskedCreditCards(credit_card.server_id());
  DeleteFromMaskedCreditCards(credit_card.server_id());

  CreditCard masked(credit_card);
  masked.set_record_type(CreditCard::MASKED_SERVER_CARD);
  masked.SetNumber(credit_card.LastFourDigits());
  masked.RecordAndLogUse();
  DCHECK(!masked.network().empty());
  AddMaskedCreditCards({masked});

  AddUnmaskedCreditCard(credit_card.server_id(), credit_card.number());

  transaction.Commit();

  return db_->GetLastChangeCount() > 0;
}

std::unique_ptr<CreditCard> AutofillTable::GetCreditCard(
    const std::string& guid) {
  DCHECK(base::IsValidGUID(guid));
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, use_count, use_date, date_modified, "
      "origin, billing_address_id "
      "FROM credit_cards "
      "WHERE guid = ?"));
  s.BindString(0, guid);

  if (!s.Step())
    return std::unique_ptr<CreditCard>();

  return CreditCardFromStatement(s, *autofill_table_encryptor_);
}

bool AutofillTable::GetCreditCards(
    std::vector<std::unique_ptr<CreditCard>>* credit_cards) {
  DCHECK(credit_cards);
  credit_cards->clear();

  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid FROM credit_cards ORDER BY date_modified DESC, guid"));

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    std::unique_ptr<CreditCard> credit_card = GetCreditCard(guid);
    if (!credit_card)
      return false;
    credit_cards->push_back(std::move(credit_card));
  }

  return s.Succeeded();
}

bool AutofillTable::GetServerCreditCards(
    std::vector<std::unique_ptr<CreditCard>>* credit_cards) const {
  credit_cards->clear();

  sql::Statement s(db_->GetUniqueStatement(
      "SELECT "
      "card_number_encrypted, "       // 0
      "last_four,"                    // 1
      "masked.id,"                    // 2
      "metadata.use_count,"           // 3
      "metadata.use_date,"            // 4
      "network,"                      // 5
      "type,"                         // 6
      "status,"                       // 7
      "name_on_card,"                 // 8
      "exp_month,"                    // 9
      "exp_year,"                     // 10
      "metadata.billing_address_id,"  // 11
      "bank_name "                    // 12
      "FROM masked_credit_cards masked "
      "LEFT OUTER JOIN unmasked_credit_cards USING (id) "
      "LEFT OUTER JOIN server_card_metadata metadata USING (id)"));
  while (s.Step()) {
    int index = 0;

    // If the card_number_encrypted field is nonempty, we can assume this card
    // is a full card, otherwise it's masked.
    base::string16 full_card_number =
        UnencryptedCardFromColumn(s, index++, *autofill_table_encryptor_);
    base::string16 last_four = s.ColumnString16(index++);
    CreditCard::RecordType record_type = full_card_number.empty()
                                             ? CreditCard::MASKED_SERVER_CARD
                                             : CreditCard::FULL_SERVER_CARD;
    std::string server_id = s.ColumnString(index++);
    std::unique_ptr<CreditCard> card =
        std::make_unique<CreditCard>(record_type, server_id);
    card->SetRawInfo(CREDIT_CARD_NUMBER,
                     record_type == CreditCard::MASKED_SERVER_CARD
                         ? last_four
                         : full_card_number);
    card->set_use_count(s.ColumnInt64(index++));
    card->set_use_date(base::Time::FromInternalValue(s.ColumnInt64(index++)));
    // Modification date is not tracked for server cards. Explicitly set it here
    // to override the default value of AutofillClock::Now().
    card->set_modification_date(base::Time());

    std::string card_network = s.ColumnString(index++);
    if (record_type == CreditCard::MASKED_SERVER_CARD) {
      // The issuer network must be set after setting the number to override the
      // autodetected issuer network.
      card->SetNetworkForMaskedCard(card_network.c_str());
    } else {
      DCHECK_EQ(CreditCard::GetCardNetwork(full_card_number), card_network);
    }

    int card_type = s.ColumnInt(index++);
    if (card_type >= CreditCard::CARD_TYPE_UNKNOWN &&
        card_type <= CreditCard::CARD_TYPE_PREPAID) {
      card->set_card_type(static_cast<CreditCard::CardType>(card_type));
    }

    card->SetServerStatus(ServerStatusStringToEnum(s.ColumnString(index++)));
    card->SetRawInfo(CREDIT_CARD_NAME_FULL, s.ColumnString16(index++));
    card->SetRawInfo(CREDIT_CARD_EXP_MONTH, s.ColumnString16(index++));
    card->SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, s.ColumnString16(index++));
    card->set_billing_address_id(s.ColumnString(index++));
    card->set_bank_name(s.ColumnString(index++));
    credit_cards->push_back(std::move(card));
  }
  return s.Succeeded();
}

void AutofillTable::SetServerCreditCards(
    const std::vector<CreditCard>& credit_cards) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return;

  // Delete all old values.
  sql::Statement masked_delete(
      db_->GetUniqueStatement("DELETE FROM masked_credit_cards"));
  masked_delete.Run();

  AddMaskedCreditCards(credit_cards);

  // Delete all items in the unmasked table that aren't in the new set.
  sql::Statement unmasked_delete(db_->GetUniqueStatement(
      "DELETE FROM unmasked_credit_cards WHERE id NOT IN "
      "(SELECT id FROM masked_credit_cards)"));
  unmasked_delete.Run();
  // Do the same for metadata.
  sql::Statement metadata_delete(db_->GetUniqueStatement(
      "DELETE FROM server_card_metadata WHERE id NOT IN "
      "(SELECT id FROM masked_credit_cards)"));
  metadata_delete.Run();

  transaction.Commit();
}

bool AutofillTable::UnmaskServerCreditCard(const CreditCard& masked,
                                           const base::string16& full_number) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  // Make sure there aren't duplicates for this card.
  DeleteFromUnmaskedCreditCards(masked.server_id());

  AddUnmaskedCreditCard(masked.server_id(), full_number);

  CreditCard unmasked = masked;
  unmasked.set_record_type(CreditCard::FULL_SERVER_CARD);
  unmasked.SetNumber(full_number);
  unmasked.RecordAndLogUse();
  UpdateServerCardMetadata(unmasked);

  transaction.Commit();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::MaskServerCreditCard(const std::string& id) {
  return DeleteFromUnmaskedCreditCards(id);
}

bool AutofillTable::AddServerCardMetadata(
    const AutofillMetadata& card_metadata) {
  sql::Statement s(
      db_->GetUniqueStatement("INSERT INTO server_card_metadata(use_count, "
                              "use_date, billing_address_id, id)"
                              "VALUES (?,?,?,?)"));
  s.BindInt64(0, card_metadata.use_count);
  s.BindInt64(1, card_metadata.use_date.ToInternalValue());
  s.BindString(2, card_metadata.billing_address_id);
  s.BindString(3, card_metadata.id);
  s.Run();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::UpdateServerCardMetadata(const CreditCard& credit_card) {
  DCHECK_NE(CreditCard::LOCAL_CARD, credit_card.record_type());

  sql::Statement remove(
      db_->GetUniqueStatement("DELETE FROM server_card_metadata WHERE id = ?"));
  remove.BindString(0, credit_card.server_id());
  remove.Run();

  sql::Statement s(
      db_->GetUniqueStatement("INSERT INTO server_card_metadata(use_count, "
                              "use_date, billing_address_id, id)"
                              "VALUES (?,?,?,?)"));
  s.BindInt64(0, credit_card.use_count());
  s.BindInt64(1, credit_card.use_date().ToInternalValue());
  s.BindString(2, credit_card.billing_address_id());
  s.BindString(3, credit_card.server_id());
  s.Run();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::UpdateServerCardMetadata(
    const AutofillMetadata& card_metadata) {
  // Do not check if there was a record that got deleted. Inserting a new one is
  // also fine.
  RemoveServerCardMetadata(card_metadata.id);
  sql::Statement s(
      db_->GetUniqueStatement("INSERT INTO server_card_metadata(use_count, "
                              "use_date, billing_address_id, id)"
                              "VALUES (?,?,?,?)"));
  s.BindInt64(0, card_metadata.use_count);
  s.BindInt64(1, card_metadata.use_date.ToInternalValue());
  s.BindString(2, card_metadata.billing_address_id);
  s.BindString(3, card_metadata.id);
  s.Run();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::RemoveServerCardMetadata(const std::string& id) {
  sql::Statement remove(
      db_->GetUniqueStatement("DELETE FROM server_card_metadata WHERE id = ?"));
  remove.BindString(0, id);
  remove.Run();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::GetServerCardsMetadata(
    std::map<std::string, AutofillMetadata>* cards_metadata) const {
  cards_metadata->clear();

  sql::Statement s(
      db_->GetUniqueStatement("SELECT id, use_count, use_date, "
                              "billing_address_id FROM server_card_metadata"));

  while (s.Step()) {
    int index = 0;

    AutofillMetadata card_metadata;
    card_metadata.id = s.ColumnString(index++);
    card_metadata.use_count = s.ColumnInt64(index++);
    card_metadata.use_date =
        base::Time::FromInternalValue(s.ColumnInt64(index++));
    card_metadata.billing_address_id = s.ColumnString(index++);
    (*cards_metadata)[card_metadata.id] = card_metadata;
  }
  return s.Succeeded();
}

bool AutofillTable::AddServerAddressMetadata(
    const AutofillMetadata& address_metadata) {
  sql::Statement s(
      db_->GetUniqueStatement("INSERT INTO server_address_metadata(use_count, "
                              "use_date, has_converted, id)"
                              "VALUES (?,?,?,?)"));
  s.BindInt64(0, address_metadata.use_count);
  s.BindInt64(1, address_metadata.use_date.ToInternalValue());
  s.BindBool(2, address_metadata.has_converted);
  s.BindString(3, address_metadata.id);
  s.Run();

  return db_->GetLastChangeCount() > 0;
}

// TODO(crbug.com/680182): Record the address conversion status when a server
// address gets converted.
bool AutofillTable::UpdateServerAddressMetadata(
    const AutofillProfile& profile) {
  DCHECK_EQ(AutofillProfile::SERVER_PROFILE, profile.record_type());

  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  sql::Statement remove(db_->GetUniqueStatement(
      "DELETE FROM server_address_metadata WHERE id = ?"));
  remove.BindString(0, profile.server_id());
  remove.Run();

  sql::Statement s(
      db_->GetUniqueStatement("INSERT INTO server_address_metadata(use_count, "
                              "use_date, has_converted, id)"
                              "VALUES (?,?,?,?)"));
  s.BindInt64(0, profile.use_count());
  s.BindInt64(1, profile.use_date().ToInternalValue());
  s.BindBool(2, profile.has_converted());
  s.BindString(3, profile.server_id());
  s.Run();

  transaction.Commit();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::UpdateServerAddressMetadata(
    const AutofillMetadata& address_metadata) {
  // Do not check if there was a record that got deleted. Inserting a new one is
  // also fine.
  RemoveServerAddressMetadata(address_metadata.id);
  sql::Statement s(
      db_->GetUniqueStatement("INSERT INTO server_address_metadata(use_count, "
                              "use_date, has_converted, id)"
                              "VALUES (?,?,?,?)"));
  s.BindInt64(0, address_metadata.use_count);
  s.BindInt64(1, address_metadata.use_date.ToInternalValue());
  s.BindBool(2, address_metadata.has_converted);
  s.BindString(3, address_metadata.id);
  s.Run();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::RemoveServerAddressMetadata(const std::string& id) {
  sql::Statement remove(db_->GetUniqueStatement(
      "DELETE FROM server_address_metadata WHERE id = ?"));
  remove.BindString(0, id);
  remove.Run();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::GetServerAddressesMetadata(
    std::map<std::string, AutofillMetadata>* addresses_metadata) const {
  addresses_metadata->clear();

  sql::Statement s(
      db_->GetUniqueStatement("SELECT id, use_count, use_date, has_converted "
                              "FROM server_address_metadata"));
  while (s.Step()) {
    int index = 0;

    AutofillMetadata address_metadata;
    address_metadata.id = s.ColumnString(index++);
    address_metadata.use_count = s.ColumnInt64(index++);
    address_metadata.use_date =
        base::Time::FromInternalValue(s.ColumnInt64(index++));
    address_metadata.has_converted = s.ColumnBool(index++);
    (*addresses_metadata)[address_metadata.id] = address_metadata;
  }
  return s.Succeeded();
}

void AutofillTable::SetServerCardsData(
    const std::vector<CreditCard>& credit_cards) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return;

  // Delete all old values.
  sql::Statement masked_delete(
      db_->GetUniqueStatement("DELETE FROM masked_credit_cards"));
  masked_delete.Run();

  // Add all the masked cards.
  sql::Statement masked_insert(
      db_->GetUniqueStatement("INSERT INTO masked_credit_cards("
                              "id,"            // 0
                              "network,"       // 1
                              "type,"          // 2
                              "status,"        // 3
                              "name_on_card,"  // 4
                              "last_four,"     // 5
                              "exp_month,"     // 6
                              "exp_year,"      // 7
                              "bank_name)"     // 8
                              "VALUES (?,?,?,?,?,?,?,?,?)"));
  for (const CreditCard& card : credit_cards) {
    DCHECK_EQ(CreditCard::MASKED_SERVER_CARD, card.record_type());
    masked_insert.BindString(0, card.server_id());
    masked_insert.BindString(1, card.network());
    masked_insert.BindInt(2, card.card_type());
    masked_insert.BindString(3,
                             ServerStatusEnumToString(card.GetServerStatus()));
    masked_insert.BindString16(4, card.GetRawInfo(CREDIT_CARD_NAME_FULL));
    masked_insert.BindString16(5, card.LastFourDigits());
    masked_insert.BindString16(6, card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
    masked_insert.BindString16(7,
                               card.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
    masked_insert.BindString(8, card.bank_name());
    masked_insert.Run();
    masked_insert.Reset(true);
  }

  // Delete all items in the unmasked table that aren't in the new set.
  sql::Statement unmasked_delete(db_->GetUniqueStatement(
      "DELETE FROM unmasked_credit_cards WHERE id NOT IN "
      "(SELECT id FROM masked_credit_cards)"));
  unmasked_delete.Run();

  transaction.Commit();
}

void AutofillTable::SetServerAddressesData(
    const std::vector<AutofillProfile>& profiles) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return;

  // Delete existing server addresses.
  sql::Statement delete_old(
      db_->GetUniqueStatement("DELETE FROM server_addresses"));
  delete_old.Run();

  // Add the new server addresses.
  sql::Statement insert(db_->GetUniqueStatement(
      "INSERT INTO server_addresses("
      "id,"
      "recipient_name,"
      "company_name,"
      "street_address,"
      "address_1,"     // ADDRESS_HOME_STATE
      "address_2,"     // ADDRESS_HOME_CITY
      "address_3,"     // ADDRESS_HOME_DEPENDENT_LOCALITY
      "address_4,"     // Not supported in AutofillProfile yet.
      "postal_code,"   // ADDRESS_HOME_ZIP
      "sorting_code,"  // ADDRESS_HOME_SORTING_CODE
      "country_code,"  // ADDRESS_HOME_COUNTRY
      "phone_number,"  // PHONE_HOME_WHOLE_NUMBER
      "language_code) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  for (const auto& profile : profiles) {
    DCHECK(profile.record_type() == AutofillProfile::SERVER_PROFILE);

    int index = 0;
    insert.BindString(index++, profile.server_id());
    insert.BindString16(index++, profile.GetRawInfo(NAME_FULL));
    insert.BindString16(index++, profile.GetRawInfo(COMPANY_NAME));
    insert.BindString16(index++,
                        profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
    insert.BindString16(index++, profile.GetRawInfo(ADDRESS_HOME_STATE));
    insert.BindString16(index++, profile.GetRawInfo(ADDRESS_HOME_CITY));
    insert.BindString16(index++,
                        profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY));
    index++;  // SKip address_4 which we haven't added to AutofillProfile yet.
    insert.BindString16(index++, profile.GetRawInfo(ADDRESS_HOME_ZIP));
    insert.BindString16(index++, profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE));
    insert.BindString16(index++, profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
    insert.BindString16(index++, profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
    insert.BindString(index++, profile.language_code());

    insert.Run();
    insert.Reset(true);
  }

  transaction.Commit();
}

void AutofillTable::SetPaymentsCustomerData(
    const PaymentsCustomerData* customer_data) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return;

  // Delete all old values.
  sql::Statement customer_data_delete(
      db_->GetUniqueStatement("DELETE FROM payments_customer_data"));
  customer_data_delete.Run();

  if (customer_data) {
    sql::Statement insert_customer_data(db_->GetUniqueStatement(
        "INSERT INTO payments_customer_data (customer_id) VALUES (?)"));
    insert_customer_data.BindString(0, customer_data->customer_id);
    insert_customer_data.Run();
  }

  transaction.Commit();
}

bool AutofillTable::GetPaymentsCustomerData(
    std::unique_ptr<PaymentsCustomerData>* customer_data) const {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT customer_id FROM payments_customer_data"));
  if (s.Step()) {
    customer_data->reset(
        new PaymentsCustomerData(/*customer_id=*/s.ColumnString(0)));
  }

  return s.Succeeded();
}

bool AutofillTable::InsertVPA(const std::string& vpa) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  sql::Statement insert_vpa_statement(
      db_->GetUniqueStatement("INSERT INTO payments_upi_vpa (vpa) VALUES (?)"));
  insert_vpa_statement.BindString(0, vpa);
  insert_vpa_statement.Run();

  transaction.Commit();

  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::ClearAllServerData() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;  // Some error, nothing was changed.

  sql::Statement masked(
      db_->GetUniqueStatement("DELETE FROM masked_credit_cards"));
  masked.Run();
  bool changed = db_->GetLastChangeCount() > 0;

  sql::Statement unmasked(
      db_->GetUniqueStatement("DELETE FROM unmasked_credit_cards"));
  unmasked.Run();
  changed |= db_->GetLastChangeCount() > 0;

  sql::Statement addresses(
      db_->GetUniqueStatement("DELETE FROM server_addresses"));
  addresses.Run();
  changed |= db_->GetLastChangeCount() > 0;

  sql::Statement card_metadata(
      db_->GetUniqueStatement("DELETE FROM server_card_metadata"));
  card_metadata.Run();
  changed |= db_->GetLastChangeCount() > 0;

  sql::Statement address_metadata(
      db_->GetUniqueStatement("DELETE FROM server_address_metadata"));
  address_metadata.Run();
  changed |= db_->GetLastChangeCount() > 0;

  sql::Statement customer_data(
      db_->GetUniqueStatement("DELETE FROM payments_customer_data"));
  customer_data.Run();
  changed |= db_->GetLastChangeCount() > 0;

  transaction.Commit();
  return changed;
}

bool AutofillTable::ClearAllLocalData() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;  // Some error, nothing was changed.

  ClearAutofillProfiles();
  bool changed = db_->GetLastChangeCount() > 0;
  ClearCreditCards();
  changed |= db_->GetLastChangeCount() > 0;

  transaction.Commit();
  return changed;
}

bool AutofillTable::RemoveAutofillDataModifiedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    std::vector<std::unique_ptr<AutofillProfile>>* profiles,
    std::vector<std::unique_ptr<CreditCard>>* credit_cards) {
  DCHECK(delete_end.is_null() || delete_begin < delete_end);

  time_t delete_begin_t = delete_begin.ToTimeT();
  time_t delete_end_t = GetEndTime(delete_end);

  // Remember Autofill profiles in the time range.
  sql::Statement s_profiles_get(db_->GetUniqueStatement(
      "SELECT guid FROM autofill_profiles "
      "WHERE date_modified >= ? AND date_modified < ?"));
  s_profiles_get.BindInt64(0, delete_begin_t);
  s_profiles_get.BindInt64(1, delete_end_t);

  profiles->clear();
  while (s_profiles_get.Step()) {
    std::string guid = s_profiles_get.ColumnString(0);
    std::unique_ptr<AutofillProfile> profile = GetAutofillProfile(guid);
    if (!profile)
      return false;
    profiles->push_back(std::move(profile));
  }
  if (!s_profiles_get.Succeeded())
    return false;

  // Remove the profile pieces.
  for (const std::unique_ptr<AutofillProfile>& profile : *profiles) {
    if (!RemoveAutofillProfilePieces(profile->guid(), db_))
      return false;
  }

  // Remove Autofill profiles in the time range.
  sql::Statement s_profiles(db_->GetUniqueStatement(
      "DELETE FROM autofill_profiles "
      "WHERE date_modified >= ? AND date_modified < ?"));
  s_profiles.BindInt64(0, delete_begin_t);
  s_profiles.BindInt64(1, delete_end_t);

  if (!s_profiles.Run())
    return false;

  // Remember Autofill credit cards in the time range.
  sql::Statement s_credit_cards_get(db_->GetUniqueStatement(
      "SELECT guid FROM credit_cards "
      "WHERE date_modified >= ? AND date_modified < ?"));
  s_credit_cards_get.BindInt64(0, delete_begin_t);
  s_credit_cards_get.BindInt64(1, delete_end_t);

  credit_cards->clear();
  while (s_credit_cards_get.Step()) {
    std::string guid = s_credit_cards_get.ColumnString(0);
    std::unique_ptr<CreditCard> credit_card = GetCreditCard(guid);
    if (!credit_card)
      return false;
    credit_cards->push_back(std::move(credit_card));
  }
  if (!s_credit_cards_get.Succeeded())
    return false;

  // Remove Autofill credit cards in the time range.
  sql::Statement s_credit_cards(db_->GetUniqueStatement(
      "DELETE FROM credit_cards "
      "WHERE date_modified >= ? AND date_modified < ?"));
  s_credit_cards.BindInt64(0, delete_begin_t);
  s_credit_cards.BindInt64(1, delete_end_t);
  if (!s_credit_cards.Run())
    return false;

  // Remove unmasked credit cards in the time range.
  sql::Statement s_unmasked_cards(
      db_->GetUniqueStatement("DELETE FROM unmasked_credit_cards "
                              "WHERE unmask_date >= ? AND unmask_date < ?"));
  s_unmasked_cards.BindInt64(0, delete_begin.ToInternalValue());
  s_unmasked_cards.BindInt64(1, delete_end.ToInternalValue());
  return s_unmasked_cards.Run();
}

bool AutofillTable::RemoveOriginURLsModifiedBetween(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    std::vector<std::unique_ptr<AutofillProfile>>* profiles) {
  DCHECK(delete_end.is_null() || delete_begin < delete_end);

  time_t delete_begin_t = delete_begin.ToTimeT();
  time_t delete_end_t = GetEndTime(delete_end);

  // Remember Autofill profiles with URL origins in the time range.
  sql::Statement s_profiles_get(db_->GetUniqueStatement(
      "SELECT guid, origin FROM autofill_profiles "
      "WHERE date_modified >= ? AND date_modified < ?"));
  s_profiles_get.BindInt64(0, delete_begin_t);
  s_profiles_get.BindInt64(1, delete_end_t);

  std::vector<std::string> profile_guids;
  while (s_profiles_get.Step()) {
    std::string guid = s_profiles_get.ColumnString(0);
    std::string origin = s_profiles_get.ColumnString(1);
    if (GURL(origin).is_valid())
      profile_guids.push_back(guid);
  }
  if (!s_profiles_get.Succeeded())
    return false;

  // Clear out the origins for the found Autofill profiles.
  for (const std::string& guid : profile_guids) {
    sql::Statement s_profile(db_->GetUniqueStatement(
        "UPDATE autofill_profiles SET origin='' WHERE guid=?"));
    s_profile.BindString(0, guid);
    if (!s_profile.Run())
      return false;

    std::unique_ptr<AutofillProfile> profile = GetAutofillProfile(guid);
    if (!profile)
      return false;

    profiles->push_back(std::move(profile));
  }

  // Remember Autofill credit cards with URL origins in the time range.
  sql::Statement s_credit_cards_get(db_->GetUniqueStatement(
      "SELECT guid, origin FROM credit_cards "
      "WHERE date_modified >= ? AND date_modified < ?"));
  s_credit_cards_get.BindInt64(0, delete_begin_t);
  s_credit_cards_get.BindInt64(1, delete_end_t);

  std::vector<std::string> credit_card_guids;
  while (s_credit_cards_get.Step()) {
    std::string guid = s_credit_cards_get.ColumnString(0);
    std::string origin = s_credit_cards_get.ColumnString(1);
    if (GURL(origin).is_valid())
      credit_card_guids.push_back(guid);
  }
  if (!s_credit_cards_get.Succeeded())
    return false;

  // Clear out the origins for the found credit cards.
  for (const std::string& guid : credit_card_guids) {
    sql::Statement s_credit_card(db_->GetUniqueStatement(
        "UPDATE credit_cards SET origin='' WHERE guid=?"));
    s_credit_card.BindString(0, guid);
    if (!s_credit_card.Run())
      return false;
  }

  return true;
}

bool AutofillTable::ClearAutofillProfiles() {
  sql::Statement s1(db_->GetUniqueStatement("DELETE FROM autofill_profiles"));

  if (!s1.Run())
    return false;

  sql::Statement s2(
      db_->GetUniqueStatement("DELETE FROM autofill_profile_names"));

  if (!s2.Run())
    return false;

  sql::Statement s3(
      db_->GetUniqueStatement("DELETE FROM autofill_profile_emails"));

  if (!s3.Run())
    return false;

  sql::Statement s4(
      db_->GetUniqueStatement("DELETE FROM autofill_profile_phones"));

  return s4.Run();
}

bool AutofillTable::ClearCreditCards() {
  sql::Statement s1(db_->GetUniqueStatement("DELETE FROM credit_cards"));
  return s1.Run();
}

bool AutofillTable::GetAllSyncMetadata(syncer::ModelType model_type,
                                       syncer::MetadataBatch* metadata_batch) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";
  DCHECK(metadata_batch);
  if (!GetAllSyncEntityMetadata(model_type, metadata_batch)) {
    return false;
  }

  sync_pb::ModelTypeState model_type_state;
  if (!GetModelTypeState(model_type, &model_type_state))
    return false;

  metadata_batch->SetModelTypeState(model_type_state);
  return true;
}

bool AutofillTable::UpdateSyncMetadata(
    syncer::ModelType model_type,
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";

  sql::Statement s(db_->GetUniqueStatement(
      "INSERT OR REPLACE INTO autofill_sync_metadata "
      "(model_type, storage_key, value) VALUES(?, ?, ?)"));
  s.BindInt(0, GetKeyValueForModelType(model_type));
  s.BindString(1, storage_key);
  s.BindString(2, metadata.SerializeAsString());

  return s.Run();
}

bool AutofillTable::ClearSyncMetadata(syncer::ModelType model_type,
                                      const std::string& storage_key) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";

  sql::Statement s(
      db_->GetUniqueStatement("DELETE FROM autofill_sync_metadata WHERE "
                              "model_type=? AND storage_key=?"));
  s.BindInt(0, GetKeyValueForModelType(model_type));
  s.BindString(1, storage_key);

  return s.Run();
}

bool AutofillTable::UpdateModelTypeState(
    syncer::ModelType model_type,
    const sync_pb::ModelTypeState& model_type_state) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";

  // Hardcode the id to force a collision, ensuring that there remains only a
  // single entry.
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT OR REPLACE INTO autofill_model_type_state (model_type, value) "
      "VALUES(?,?)"));
  s.BindInt(0, GetKeyValueForModelType(model_type));
  s.BindString(1, model_type_state.SerializeAsString());

  return s.Run();
}

bool AutofillTable::ClearModelTypeState(syncer::ModelType model_type) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";

  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM autofill_model_type_state WHERE model_type=?"));
  s.BindInt(0, GetKeyValueForModelType(model_type));

  return s.Run();
}

bool AutofillTable::RemoveOrphanAutofillTableRows() {
  // Get all the orphan guids.
  std::set<std::string> orphan_guids;
  sql::Statement s_orphan_profile_pieces_get(db_->GetUniqueStatement(
      "SELECT guid FROM (SELECT guid FROM autofill_profile_names UNION SELECT "
      "guid FROM autofill_profile_emails UNION SELECT guid FROM "
      "autofill_profile_phones) WHERE guid NOT IN (SELECT guid FROM "
      "autofill_profiles)"));

  // Put the orphan guids in a set.
  while (s_orphan_profile_pieces_get.Step())
    orphan_guids.insert(s_orphan_profile_pieces_get.ColumnString(0));

  if (!s_orphan_profile_pieces_get.Succeeded())
    return false;

  // Remove the profile pieces for the orphan guids.
  for (const std::string& guid : orphan_guids) {
    if (!RemoveAutofillProfilePieces(guid, db_))
      return false;
  }

  return true;
}

bool AutofillTable::MigrateToVersion54AddI18nFieldsAndRemoveDeprecatedFields() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  // Test the existence of the |address_line_1| column as an indication that a
  // migration is needed.  It is possible that the new |autofill_profile_phones|
  // schema is in place because the table was newly created when migrating from
  // a pre-version-23 database.
  if (db_->DoesColumnExist("autofill_profiles", "address_line_1")) {
    // Create a temporary copy of the autofill_profiles table in the (newer)
    // version 54 format.  This table
    //   (a) adds columns for street_address, dependent_locality, and
    //       sorting_code,
    //   (b) removes the address_line_1 and address_line_2 columns, which are
    //       replaced by the street_address column, and
    //   (c) removes the country column, which was long deprecated.
    if (db_->DoesTableExist("autofill_profiles_temp") ||
        !db_->Execute("CREATE TABLE autofill_profiles_temp ( "
                      "guid VARCHAR PRIMARY KEY, "
                      "company_name VARCHAR, "
                      "street_address VARCHAR, "
                      "dependent_locality VARCHAR, "
                      "city VARCHAR, "
                      "state VARCHAR, "
                      "zipcode VARCHAR, "
                      "sorting_code VARCHAR, "
                      "country_code VARCHAR, "
                      "date_modified INTEGER NOT NULL DEFAULT 0, "
                      "origin VARCHAR DEFAULT '')")) {
      return false;
    }

    // Copy over the data from the autofill_profiles table, taking care to merge
    // the address lines 1 and 2 into the new street_address column.
    if (!db_->Execute("INSERT INTO autofill_profiles_temp "
                      "SELECT guid, company_name, '', '', city, state, zipcode,"
                      " '', country_code, date_modified, origin "
                      "FROM autofill_profiles")) {
      return false;
    }
    sql::Statement s(db_->GetUniqueStatement(
        "SELECT guid, address_line_1, address_line_2 FROM autofill_profiles"));
    while (s.Step()) {
      std::string guid = s.ColumnString(0);
      base::string16 line1 = s.ColumnString16(1);
      base::string16 line2 = s.ColumnString16(2);
      base::string16 street_address = line1;
      if (!line2.empty())
        street_address += base::ASCIIToUTF16("\n") + line2;

      sql::Statement s_update(db_->GetUniqueStatement(
          "UPDATE autofill_profiles_temp SET street_address=? WHERE guid=?"));
      s_update.BindString16(0, street_address);
      s_update.BindString(1, guid);
      if (!s_update.Run())
        return false;
    }
    if (!s.Succeeded())
      return false;

    // Delete the existing (version 53) table and replace it with the contents
    // of the temporary table.
    if (!db_->Execute("DROP TABLE autofill_profiles") ||
        !db_->Execute("ALTER TABLE autofill_profiles_temp "
                      "RENAME TO autofill_profiles")) {
      return false;
    }
  }

  // Test the existence of the |type| column as an indication that a migration
  // is needed.  It is possible that the new |autofill_profile_phones| schema is
  // in place because the table was newly created when migrating from a
  // pre-version-23 database.
  if (db_->DoesColumnExist("autofill_profile_phones", "type")) {
    // Create a temporary copy of the autofill_profile_phones table in the
    // (newer) version 54 format.  This table removes the deprecated |type|
    // column.
    if (db_->DoesTableExist("autofill_profile_phones_temp") ||
        !db_->Execute("CREATE TABLE autofill_profile_phones_temp ( "
                      "guid VARCHAR, "
                      "number VARCHAR)")) {
      return false;
    }

    // Copy over the data from the autofill_profile_phones table.
    if (!db_->Execute("INSERT INTO autofill_profile_phones_temp "
                      "SELECT guid, number FROM autofill_profile_phones")) {
      return false;
    }

    // Delete the existing (version 53) table and replace it with the contents
    // of the temporary table.
    if (!db_->Execute("DROP TABLE autofill_profile_phones"))
      return false;
    if (!db_->Execute("ALTER TABLE autofill_profile_phones_temp "
                      "RENAME TO autofill_profile_phones")) {
      return false;
    }
  }

  return transaction.Commit();
}

bool AutofillTable::MigrateToVersion55MergeAutofillDatesTable() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  if (db_->DoesTableExist("autofill_temp") ||
      !db_->Execute("CREATE TABLE autofill_temp ("
                    "name VARCHAR, "
                    "value VARCHAR, "
                    "value_lower VARCHAR, "
                    "date_created INTEGER DEFAULT 0, "
                    "date_last_used INTEGER DEFAULT 0, "
                    "count INTEGER DEFAULT 1, "
                    "PRIMARY KEY (name, value))")) {
    return false;
  }

  // Slurp up the data from the existing table and write it to the new table.
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT name, value, value_lower, count, MIN(date_created),"
      " MAX(date_created) "
      "FROM autofill a JOIN autofill_dates ad ON a.pair_id=ad.pair_id "
      "GROUP BY name, value, value_lower, count"));
  while (s.Step()) {
    sql::Statement s_insert(db_->GetUniqueStatement(
        "INSERT INTO autofill_temp "
        "(name, value, value_lower, count, date_created, date_last_used) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    s_insert.BindString16(0, s.ColumnString16(0));
    s_insert.BindString16(1, s.ColumnString16(1));
    s_insert.BindString16(2, s.ColumnString16(2));
    s_insert.BindInt(3, s.ColumnInt(3));
    s_insert.BindInt64(4, s.ColumnInt64(4));
    s_insert.BindInt64(5, s.ColumnInt64(5));
    if (!s_insert.Run())
      return false;
  }

  if (!s.Succeeded())
    return false;

  // Delete the existing (version 54) tables and replace them with the contents
  // of the temporary table.
  if (!db_->Execute("DROP TABLE autofill") ||
      !db_->Execute("DROP TABLE autofill_dates") ||
      !db_->Execute("ALTER TABLE autofill_temp "
                    "RENAME TO autofill")) {
    return false;
  }

  // Create indices on the new table, for fast lookups.
  if (!db_->Execute("CREATE INDEX autofill_name ON autofill (name)") ||
      !db_->Execute("CREATE INDEX autofill_name_value_lower ON "
                    "autofill (name, value_lower)")) {
    return false;
  }

  return transaction.Commit();
}

bool AutofillTable::MigrateToVersion56AddProfileLanguageCodeForFormatting() {
  return db_->Execute(
      "ALTER TABLE autofill_profiles ADD COLUMN language_code VARCHAR");
}

bool AutofillTable::MigrateToVersion57AddFullNameField() {
  return db_->Execute(
      "ALTER TABLE autofill_profile_names ADD COLUMN full_name VARCHAR");
}

bool AutofillTable::MigrateToVersion60AddServerCards() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  if (!db_->DoesTableExist("masked_credit_cards") &&
      !db_->Execute("CREATE TABLE masked_credit_cards ("
                    "id VARCHAR,"
                    "status VARCHAR,"
                    "name_on_card VARCHAR,"
                    "type VARCHAR,"
                    "last_four VARCHAR,"
                    "exp_month INTEGER DEFAULT 0,"
                    "exp_year INTEGER DEFAULT 0)")) {
    return false;
  }

  if (!db_->DoesTableExist("unmasked_credit_cards") &&
      !db_->Execute("CREATE TABLE unmasked_credit_cards ("
                    "id VARCHAR,"
                    "card_number_encrypted VARCHAR)")) {
    return false;
  }

  if (!db_->DoesTableExist("server_addresses") &&
      !db_->Execute("CREATE TABLE server_addresses ("
                    "id VARCHAR,"
                    "company_name VARCHAR,"
                    "street_address VARCHAR,"
                    "address_1 VARCHAR,"
                    "address_2 VARCHAR,"
                    "address_3 VARCHAR,"
                    "address_4 VARCHAR,"
                    "postal_code VARCHAR,"
                    "sorting_code VARCHAR,"
                    "country_code VARCHAR,"
                    "language_code VARCHAR)")) {
    return false;
  }

  return transaction.Commit();
}

bool AutofillTable::MigrateToVersion61AddUsageStats() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  // Add use_count to autofill_profiles.
  if (!db_->DoesColumnExist("autofill_profiles", "use_count") &&
      !db_->Execute("ALTER TABLE autofill_profiles ADD COLUMN "
                    "use_count INTEGER NOT NULL DEFAULT 0")) {
    return false;
  }

  // Add use_date to autofill_profiles.
  if (!db_->DoesColumnExist("autofill_profiles", "use_date") &&
      !db_->Execute("ALTER TABLE autofill_profiles ADD COLUMN "
                    "use_date INTEGER NOT NULL DEFAULT 0")) {
    return false;
  }

  // Add use_count to credit_cards.
  if (!db_->DoesColumnExist("credit_cards", "use_count") &&
      !db_->Execute("ALTER TABLE credit_cards ADD COLUMN "
                    "use_count INTEGER NOT NULL DEFAULT 0")) {
    return false;
  }

  // Add use_date to credit_cards.
  if (!db_->DoesColumnExist("credit_cards", "use_date") &&
      !db_->Execute("ALTER TABLE credit_cards ADD COLUMN "
                    "use_date INTEGER NOT NULL DEFAULT 0")) {
    return false;
  }

  return transaction.Commit();
}

bool AutofillTable::MigrateToVersion62AddUsageStatsForUnmaskedCards() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  // Add use_count to unmasked_credit_cards.
  if (!db_->DoesColumnExist("unmasked_credit_cards", "use_count") &&
      !db_->Execute("ALTER TABLE unmasked_credit_cards ADD COLUMN "
                    "use_count INTEGER NOT NULL DEFAULT 0")) {
    return false;
  }

  // Add use_date to unmasked_credit_cards.
  if (!db_->DoesColumnExist("unmasked_credit_cards", "use_date") &&
      !db_->Execute("ALTER TABLE unmasked_credit_cards ADD COLUMN "
                    "use_date INTEGER NOT NULL DEFAULT 0")) {
    return false;
  }

  return transaction.Commit();
}

bool AutofillTable::MigrateToVersion63AddServerRecipientName() {
  if (!db_->DoesColumnExist("server_addresses", "recipient_name") &&
      !db_->Execute("ALTER TABLE server_addresses ADD COLUMN "
                    "recipient_name VARCHAR")) {
    return false;
  }
  return true;
}

bool AutofillTable::MigrateToVersion64AddUnmaskDate() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  if (!db_->DoesColumnExist("unmasked_credit_cards", "unmask_date") &&
      !db_->Execute("ALTER TABLE unmasked_credit_cards ADD COLUMN "
                    "unmask_date INTEGER NOT NULL DEFAULT 0")) {
    return false;
  }
  if (!db_->DoesColumnExist("server_addresses", "phone_number") &&
      !db_->Execute("ALTER TABLE server_addresses ADD COLUMN "
                    "phone_number VARCHAR")) {
    return false;
  }

  return transaction.Commit();
}

bool AutofillTable::MigrateToVersion65AddServerMetadataTables() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  if (!db_->DoesTableExist("server_card_metadata") &&
      !db_->Execute("CREATE TABLE server_card_metadata ("
                    "id VARCHAR NOT NULL,"
                    "use_count INTEGER NOT NULL DEFAULT 0, "
                    "use_date INTEGER NOT NULL DEFAULT 0)")) {
    return false;
  }

  // This clobbers existing usage metadata, which is not synced and only
  // applies to unmasked cards. Trying to migrate the usage metadata would be
  // tricky as multiple devices for the same user get DB upgrades.
  if (!db_->Execute("UPDATE unmasked_credit_cards "
                    "SET use_count=0, use_date=0")) {
    return false;
  }

  if (!db_->DoesTableExist("server_address_metadata") &&
      !db_->Execute("CREATE TABLE server_address_metadata ("
                    "id VARCHAR NOT NULL,"
                    "use_count INTEGER NOT NULL DEFAULT 0, "
                    "use_date INTEGER NOT NULL DEFAULT 0)")) {
    return false;
  }

  // Get existing server addresses and generate IDs for them.
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT "
      "id,"
      "recipient_name,"
      "company_name,"
      "street_address,"
      "address_1,"     // ADDRESS_HOME_STATE
      "address_2,"     // ADDRESS_HOME_CITY
      "address_3,"     // ADDRESS_HOME_DEPENDENT_LOCALITY
      "address_4,"     // Not supported in AutofillProfile yet.
      "postal_code,"   // ADDRESS_HOME_ZIP
      "sorting_code,"  // ADDRESS_HOME_SORTING_CODE
      "country_code,"  // ADDRESS_HOME_COUNTRY
      "phone_number,"  // PHONE_HOME_WHOLE_NUMBER
      "language_code "
      "FROM server_addresses addresses"));
  std::vector<AutofillProfile> profiles;
  while (s.Step()) {
    int index = 0;
    AutofillProfile profile(AutofillProfile::SERVER_PROFILE,
                            s.ColumnString(index++));

    base::string16 recipient_name = s.ColumnString16(index++);
    profile.SetRawInfo(COMPANY_NAME, s.ColumnString16(index++));
    profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, s.ColumnString16(index++));
    profile.SetRawInfo(ADDRESS_HOME_STATE, s.ColumnString16(index++));
    profile.SetRawInfo(ADDRESS_HOME_CITY, s.ColumnString16(index++));
    profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                       s.ColumnString16(index++));
    index++;  // Skip address_4 which we haven't added to AutofillProfile yet.
    profile.SetRawInfo(ADDRESS_HOME_ZIP, s.ColumnString16(index++));
    profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE, s.ColumnString16(index++));
    profile.SetRawInfo(ADDRESS_HOME_COUNTRY, s.ColumnString16(index++));
    base::string16 phone_number = s.ColumnString16(index++);
    profile.set_language_code(s.ColumnString(index++));
    profile.SetInfo(NAME_FULL, recipient_name, profile.language_code());
    profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, phone_number,
                    profile.language_code());
    profile.GenerateServerProfileIdentifier();
    profiles.push_back(profile);
  }

  // Reinsert with the generated IDs.
  sql::Statement delete_old(
      db_->GetUniqueStatement("DELETE FROM server_addresses"));
  delete_old.Run();

  sql::Statement insert(db_->GetUniqueStatement(
      "INSERT INTO server_addresses("
      "id,"
      "recipient_name,"
      "company_name,"
      "street_address,"
      "address_1,"     // ADDRESS_HOME_STATE
      "address_2,"     // ADDRESS_HOME_CITY
      "address_3,"     // ADDRESS_HOME_DEPENDENT_LOCALITY
      "address_4,"     // Not supported in AutofillProfile yet.
      "postal_code,"   // ADDRESS_HOME_ZIP
      "sorting_code,"  // ADDRESS_HOME_SORTING_CODE
      "country_code,"  // ADDRESS_HOME_COUNTRY
      "phone_number,"  // PHONE_HOME_WHOLE_NUMBER
      "language_code) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  for (const AutofillProfile& profile : profiles) {
    int index = 0;
    insert.BindString(index++, profile.server_id());
    insert.BindString16(index++, profile.GetRawInfo(NAME_FULL));
    insert.BindString16(index++, profile.GetRawInfo(COMPANY_NAME));
    insert.BindString16(index++,
                        profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
    insert.BindString16(index++, profile.GetRawInfo(ADDRESS_HOME_STATE));
    insert.BindString16(index++, profile.GetRawInfo(ADDRESS_HOME_CITY));
    insert.BindString16(index++,
                        profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY));
    index++;  // SKip address_4 which we haven't added to AutofillProfile yet.
    insert.BindString16(index++, profile.GetRawInfo(ADDRESS_HOME_ZIP));
    insert.BindString16(index++, profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE));
    insert.BindString16(index++, profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
    insert.BindString16(index++, profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
    insert.BindString(index++, profile.language_code());
    insert.Run();
    insert.Reset(true);
  }

  return transaction.Commit();
}

bool AutofillTable::MigrateToVersion66AddCardBillingAddress() {
  // The default value for this column is null, but Connection::ColumnString()
  // returns an empty string for that.
  return db_->Execute(
      "ALTER TABLE credit_cards ADD COLUMN billing_address_id VARCHAR");
}

bool AutofillTable::MigrateToVersion67AddMaskedCardBillingAddress() {
  // The default value for this column is null, but Connection::ColumnString()
  // returns an empty string for that.
  return db_->Execute(
      "ALTER TABLE masked_credit_cards ADD COLUMN billing_address_id VARCHAR");
}

bool AutofillTable::MigrateToVersion70AddSyncMetadata() {
  if (!db_->Execute("CREATE TABLE autofill_sync_metadata ("
                    "storage_key VARCHAR PRIMARY KEY NOT NULL,"
                    "value BLOB)")) {
    return false;
  }
  return db_->Execute(
      "CREATE TABLE autofill_model_type_state (id INTEGER PRIMARY KEY, value "
      "BLOB)");
}

bool AutofillTable::
    MigrateToVersion71AddHasConvertedAndBillingAddressIdMetadata() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  // Add the new has_converted column to the server_address_metadata table.
  if (!db_->DoesColumnExist("server_address_metadata", "has_converted") &&
      !db_->Execute("ALTER TABLE server_address_metadata ADD COLUMN "
                    "has_converted BOOL NOT NULL DEFAULT FALSE")) {
    return false;
  }

  // Add the new billing_address_id column to the server_card_metadata table.
  if (!db_->DoesColumnExist("server_card_metadata", "billing_address_id") &&
      !db_->Execute("ALTER TABLE server_card_metadata ADD COLUMN "
                    "billing_address_id VARCHAR")) {
    return false;
  }

  // Copy over the billing_address_id from the masked_server_cards to
  // server_card_metadata.
  if (!db_->Execute("UPDATE server_card_metadata "
                    "SET billing_address_id = "
                    "(SELECT billing_address_id "
                    "FROM masked_credit_cards "
                    "WHERE id = server_card_metadata.id)")) {
    return false;
  }

  if (db_->DoesTableExist("masked_credit_cards_temp") &&
      !db_->Execute("DROP TABLE masked_credit_cards_temp")) {
    return false;
  }

  // Remove the billing_address_id column from the masked_credit_cards table.
  // Create a temporary table that is a copy of masked_credit_cards but without
  // the billing_address_id column.
  if (!db_->Execute("CREATE TABLE masked_credit_cards_temp ("
                    "id VARCHAR,"
                    "status VARCHAR,"
                    "name_on_card VARCHAR,"
                    "type VARCHAR,"
                    "last_four VARCHAR,"
                    "exp_month INTEGER DEFAULT 0,"
                    "exp_year INTEGER DEFAULT 0)")) {
    return false;
  }
  // Copy over the data from the original masked_credit_cards table.
  if (!db_->Execute("INSERT INTO masked_credit_cards_temp "
                    "SELECT id, status, name_on_card, type, last_four, "
                    "exp_month, exp_year "
                    "FROM masked_credit_cards")) {
    return false;
  }
  // Delete the existing table and replace it with the contents of the
  // temporary table.
  if (!db_->Execute("DROP TABLE masked_credit_cards") ||
      !db_->Execute("ALTER TABLE masked_credit_cards_temp "
                    "RENAME TO masked_credit_cards")) {
    return false;
  }

  return transaction.Commit();
}

bool AutofillTable::MigrateToVersion72RenameCardTypeToIssuerNetwork() {
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  if (db_->DoesTableExist("masked_credit_cards_temp") &&
      !db_->Execute("DROP TABLE masked_credit_cards_temp")) {
    return false;
  }

  return db_->Execute(
             "CREATE TABLE masked_credit_cards_temp ("
             "id VARCHAR,"
             "status VARCHAR,"
             "name_on_card VARCHAR,"
             "network VARCHAR,"
             "last_four VARCHAR,"
             "exp_month INTEGER DEFAULT 0,"
             "exp_year INTEGER DEFAULT 0)") &&
         db_->Execute(
             "INSERT INTO masked_credit_cards_temp ("
             "id, status, name_on_card, network, last_four, exp_month, exp_year"
             ") SELECT "
             "id, status, name_on_card, type,    last_four, exp_month, exp_year"
             " FROM masked_credit_cards") &&
         db_->Execute("DROP TABLE masked_credit_cards") &&
         db_->Execute(
             "ALTER TABLE masked_credit_cards_temp "
             "RENAME TO masked_credit_cards") &&
         transaction.Commit();
}

bool AutofillTable::MigrateToVersion73AddMaskedCardBankName() {
  // Add the new bank_name column to the masked_credit_cards table.
  return db_->DoesColumnExist("masked_credit_cards", "bank_name") ||
         db_->Execute(
             "ALTER TABLE masked_credit_cards ADD COLUMN bank_name VARCHAR");
}

bool AutofillTable::MigrateToVersion74AddServerCardTypeColumn() {
  // Version 73 was actually used by two different schemas; an attempt to add
  // the "type" column (as in this version 74) was landed and reverted, and then
  // the "bank_name" column was added (and stuck). Some clients may have been
  // upgraded to one and some the other. Figure out which is the case.
  const bool added_type_column_in_v73 =
      db_->DoesColumnExist("masked_credit_cards", "type");

  // If we previously added the "type" column, then it's already present with
  // the correct semantics, but we now need to run the "bank_name" migration.
  // Otherwise, we need to add "type" now.
  return added_type_column_in_v73 ? MigrateToVersion73AddMaskedCardBankName()
                                  : db_->Execute(
                                        "ALTER TABLE masked_credit_cards ADD "
                                        "COLUMN type INTEGER DEFAULT 0");
}

bool AutofillTable::MigrateToVersion75AddProfileValidityBitfieldColumn() {
  // Add the new valdity_bitfield column to the autofill_profiles table.
  return db_->Execute(
      "ALTER TABLE autofill_profiles ADD COLUMN validity_bitfield UNSIGNED NOT "
      "NULL DEFAULT 0");
}

bool AutofillTable::MigrateToVersion78AddModelTypeColumns() {
  // Add the new model type columns to the autofill metadata tables.
  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  if (db_->DoesTableExist("autofill_sync_metadata_temp") &&
      !db_->Execute("DROP TABLE autofill_sync_metadata_temp")) {
    return false;
  }

  if (db_->DoesTableExist("autofill_model_type_state_temp") &&
      !db_->Execute("DROP TABLE autofill_model_type_state_temp")) {
    return false;
  }

  if (!db_->Execute("CREATE TABLE autofill_sync_metadata_temp ("
                    "model_type INTEGER NOT NULL, "
                    "storage_key VARCHAR NOT NULL, "
                    "value BLOB, "
                    "PRIMARY KEY (model_type, storage_key))") ||
      !db_->Execute("CREATE TABLE autofill_model_type_state_temp ("
                    "model_type INTEGER NOT NULL PRIMARY KEY, value BLOB)")) {
    return false;
  }

  sql::Statement insert_metadata(
      db_->GetUniqueStatement("INSERT INTO autofill_sync_metadata_temp "
                              "(model_type, storage_key, value) "
                              "SELECT ?, storage_key, value "
                              "FROM autofill_sync_metadata"));
  // Note: This uses the *wrong* ID for the ModelType - instead of
  // |syncer::ModelTypeHistogramValue|, this should be |GetKeyValueForModelType|
  // aka |syncer::ModelTypeToStableIdentifier|. But at this point, fixing it
  // here would just make an even bigger mess. Instead, we clean this up in the
  // migration to version 81. See also crbug.com/895826.
  insert_metadata.BindInt(
      0, static_cast<int>(syncer::ModelTypeHistogramValue(syncer::AUTOFILL)));

  // Prior to this migration, the table was a singleton, containing only one
  // entry with id being hard-coded to 1.
  sql::Statement insert_state(
      db_->GetUniqueStatement("INSERT INTO autofill_model_type_state_temp "
                              "(model_type, value) SELECT ?, value "
                              "FROM autofill_model_type_state WHERE id=1"));
  // Note: Like above, this uses the *wrong* ID for the ModelType.
  insert_state.BindInt(
      0, static_cast<int>(syncer::ModelTypeHistogramValue(syncer::AUTOFILL)));

  if (!insert_metadata.Run() || !insert_state.Run()) {
    return false;
  }

  return db_->Execute("DROP TABLE autofill_sync_metadata") &&
         db_->Execute(
             "ALTER TABLE autofill_sync_metadata_temp "
             "RENAME TO autofill_sync_metadata") &&
         db_->Execute("DROP TABLE autofill_model_type_state") &&
         db_->Execute(
             "ALTER TABLE autofill_model_type_state_temp "
             "RENAME TO autofill_model_type_state") &&
         transaction.Commit();
}

bool AutofillTable::MigrateToVersion80AddIsClientValidityStatesUpdatedColumn() {
  // Add the client validity states updated flag column to the autofill_profiles
  // table.
  return db_->Execute(
      "ALTER TABLE autofill_profiles ADD COLUMN "
      "is_client_validity_states_updated BOOL NOT "
      "NULL DEFAULT FALSE");
}

bool AutofillTable::MigrateToVersion81CleanUpWrongModelTypeData() {
  // The migration to version 78 inserted Sync data with wrong values in the
  // model_type column of the autofill_model_type_state and
  // autofill_sync_metadata tables. Here we just delete the bad data - no point
  // in trying to recover anything, since by now it'll have been redownloaded
  // anyway.
  const int bad_model_type_id =
      static_cast<int>(syncer::ModelTypeHistogramValue(syncer::AUTOFILL));
  DCHECK_NE(bad_model_type_id, GetKeyValueForModelType(syncer::AUTOFILL));

  sql::Transaction transaction(db_);
  if (!transaction.Begin())
    return false;

  sql::Statement delete_bad_model_type_state(db_->GetUniqueStatement(
      "DELETE FROM autofill_model_type_state WHERE model_type = ?;"));
  delete_bad_model_type_state.BindInt(0, bad_model_type_id);

  sql::Statement delete_bad_sync_metadata(db_->GetUniqueStatement(
      "DELETE FROM autofill_sync_metadata WHERE model_type = ?;"));
  delete_bad_sync_metadata.BindInt(0, bad_model_type_id);

  return delete_bad_model_type_state.Run() && delete_bad_sync_metadata.Run() &&
         transaction.Commit();
}

bool AutofillTable::AddFormFieldValuesTime(
    const std::vector<FormFieldData>& elements,
    std::vector<AutofillChange>* changes,
    base::Time time) {
  // Only add one new entry for each unique element name.  Use |seen_names| to
  // track this.  Add up to |kMaximumUniqueNames| unique entries per form.
  const size_t kMaximumUniqueNames = 256;
  std::set<base::string16> seen_names;
  bool result = true;
  for (const FormFieldData& element : elements) {
    if (seen_names.size() >= kMaximumUniqueNames)
      break;
    if (base::Contains(seen_names, element.name))
      continue;
    result = result && AddFormFieldValueTime(element, changes, time);
    seen_names.insert(element.name);
  }
  return result;
}

bool AutofillTable::AddFormFieldValueTime(const FormFieldData& element,
                                          std::vector<AutofillChange>* changes,
                                          base::Time time) {
  sql::Statement s_exists(db_->GetUniqueStatement(
      "SELECT COUNT(*) FROM autofill WHERE name = ? AND value = ?"));
  s_exists.BindString16(0, element.name);
  s_exists.BindString16(1, element.value);
  if (!s_exists.Step())
    return false;

  bool already_exists = s_exists.ColumnInt(0) > 0;
  if (already_exists) {
    sql::Statement s(db_->GetUniqueStatement(
        "UPDATE autofill SET date_last_used = ?, count = count + 1 "
        "WHERE name = ? AND value = ?"));
    s.BindInt64(0, time.ToTimeT());
    s.BindString16(1, element.name);
    s.BindString16(2, element.value);
    if (!s.Run())
      return false;
  } else {
    time_t time_as_time_t = time.ToTimeT();
    sql::Statement s(db_->GetUniqueStatement(
        "INSERT INTO autofill "
        "(name, value, value_lower, date_created, date_last_used, count) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    s.BindString16(0, element.name);
    s.BindString16(1, element.value);
    s.BindString16(2, base::i18n::ToLower(element.value));
    s.BindInt64(3, time_as_time_t);
    s.BindInt64(4, time_as_time_t);
    s.BindInt(5, 1);
    if (!s.Run())
      return false;
  }

  AutofillChange::Type change_type =
      already_exists ? AutofillChange::UPDATE : AutofillChange::ADD;
  changes->push_back(
      AutofillChange(change_type, AutofillKey(element.name, element.value)));
  return true;
}

bool AutofillTable::SupportsMetadataForModelType(
    syncer::ModelType model_type) const {
  return (model_type == syncer::AUTOFILL ||
          model_type == syncer::AUTOFILL_PROFILE ||
          model_type == syncer::AUTOFILL_WALLET_DATA ||
          model_type == syncer::AUTOFILL_WALLET_METADATA);
}

int AutofillTable::GetKeyValueForModelType(syncer::ModelType model_type) const {
  return syncer::ModelTypeToStableIdentifier(model_type);
}

bool AutofillTable::GetAllSyncEntityMetadata(
    syncer::ModelType model_type,
    syncer::MetadataBatch* metadata_batch) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";
  DCHECK(metadata_batch);

  sql::Statement s(
      db_->GetUniqueStatement("SELECT storage_key, value FROM "
                              "autofill_sync_metadata WHERE model_type=?"));
  s.BindInt(0, GetKeyValueForModelType(model_type));

  while (s.Step()) {
    std::string storage_key = s.ColumnString(0);
    std::string serialized_metadata = s.ColumnString(1);
    auto entity_metadata = std::make_unique<sync_pb::EntityMetadata>();
    if (entity_metadata->ParseFromString(serialized_metadata)) {
      metadata_batch->AddMetadata(storage_key, std::move(entity_metadata));
    } else {
      DLOG(WARNING) << "Failed to deserialize AUTOFILL model type "
                       "sync_pb::EntityMetadata.";
      return false;
    }
  }
  return true;
}

bool AutofillTable::GetModelTypeState(syncer::ModelType model_type,
                                      sync_pb::ModelTypeState* state) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";

  sql::Statement s(db_->GetUniqueStatement(
      "SELECT value FROM autofill_model_type_state WHERE model_type=?"));
  s.BindInt(0, GetKeyValueForModelType(model_type));

  if (!s.Step()) {
    return true;
  }

  std::string serialized_state = s.ColumnString(0);
  return state->ParseFromString(serialized_state);
}

bool AutofillTable::InsertAutofillEntry(const AutofillEntry& entry) {
  std::string sql =
      "INSERT INTO autofill "
      "(name, value, value_lower, date_created, date_last_used, count) "
      "VALUES (?, ?, ?, ?, ?, ?)";
  sql::Statement s(db_->GetUniqueStatement(sql.c_str()));
  s.BindString16(0, entry.key().name());
  s.BindString16(1, entry.key().value());
  s.BindString16(2, base::i18n::ToLower(entry.key().value()));
  s.BindInt64(3, entry.date_created().ToTimeT());
  s.BindInt64(4, entry.date_last_used().ToTimeT());
  // TODO(isherman): The counts column is currently synced implicitly as the
  // number of timestamps.  Sync the value explicitly instead, since the DB now
  // only saves the first and last timestamp, which makes counting timestamps
  // completely meaningless as a way to track frequency of usage.
  s.BindInt(5, entry.date_last_used() == entry.date_created() ? 1 : 2);
  return s.Run();
}

bool AutofillTable::IsAutofillProfilesTrashEmpty() {
  sql::Statement s(
      db_->GetUniqueStatement("SELECT guid FROM autofill_profiles_trash"));

  return !s.Step();
}

bool AutofillTable::IsAutofillGUIDInTrash(const std::string& guid) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT guid FROM autofill_profiles_trash WHERE guid = ?"));
  s.BindString(0, guid);

  return s.Step();
}

void AutofillTable::AddMaskedCreditCards(
    const std::vector<CreditCard>& credit_cards) {
  DCHECK_GT(db_->transaction_nesting(), 0);
  sql::Statement masked_insert(
      db_->GetUniqueStatement("INSERT INTO masked_credit_cards("
                              "id,"            // 0
                              "network,"       // 1
                              "type,"          // 2
                              "status,"        // 3
                              "name_on_card,"  // 4
                              "last_four,"     // 5
                              "exp_month,"     // 6
                              "exp_year,"      // 7
                              "bank_name)"     // 8
                              "VALUES (?,?,?,?,?,?,?,?,?)"));
  for (const CreditCard& card : credit_cards) {
    DCHECK_EQ(CreditCard::MASKED_SERVER_CARD, card.record_type());
    masked_insert.BindString(0, card.server_id());
    masked_insert.BindString(1, card.network());
    masked_insert.BindInt(2, card.card_type());
    masked_insert.BindString(3,
                             ServerStatusEnumToString(card.GetServerStatus()));
    masked_insert.BindString16(4, card.GetRawInfo(CREDIT_CARD_NAME_FULL));
    masked_insert.BindString16(5, card.LastFourDigits());
    masked_insert.BindString16(6, card.GetRawInfo(CREDIT_CARD_EXP_MONTH));
    masked_insert.BindString16(7,
                               card.GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR));
    masked_insert.BindString(8, card.bank_name());
    masked_insert.Run();
    masked_insert.Reset(true);

    // Save the use count and use date of the card.
    UpdateServerCardMetadata(card);
  }
}

void AutofillTable::AddUnmaskedCreditCard(const std::string& id,
                                          const base::string16& full_number) {
  sql::Statement s(
      db_->GetUniqueStatement("INSERT INTO unmasked_credit_cards("
                              "id,"
                              "card_number_encrypted,"
                              "unmask_date)"
                              "VALUES (?,?,?)"));
  s.BindString(0, id);

  std::string encrypted_data;
  autofill_table_encryptor_->EncryptString16(full_number, &encrypted_data);
  s.BindBlob(1, encrypted_data.data(),
             static_cast<int>(encrypted_data.length()));
  s.BindInt64(2, AutofillClock::Now().ToInternalValue());  // unmask_date

  s.Run();
}

bool AutofillTable::DeleteFromMaskedCreditCards(const std::string& id) {
  sql::Statement s(
      db_->GetUniqueStatement("DELETE FROM masked_credit_cards WHERE id = ?"));
  s.BindString(0, id);
  s.Run();
  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::DeleteFromUnmaskedCreditCards(const std::string& id) {
  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM unmasked_credit_cards WHERE id = ?"));
  s.BindString(0, id);
  s.Run();
  return db_->GetLastChangeCount() > 0;
}

bool AutofillTable::InitMainTable() {
  if (!db_->DoesTableExist("autofill")) {
    if (!db_->Execute("CREATE TABLE autofill ("
                      "name VARCHAR, "
                      "value VARCHAR, "
                      "value_lower VARCHAR, "
                      "date_created INTEGER DEFAULT 0, "
                      "date_last_used INTEGER DEFAULT 0, "
                      "count INTEGER DEFAULT 1, "
                      "PRIMARY KEY (name, value))") ||
        !db_->Execute("CREATE INDEX autofill_name ON autofill (name)") ||
        !db_->Execute("CREATE INDEX autofill_name_value_lower ON "
                      "autofill (name, value_lower)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitCreditCardsTable() {
  if (!db_->DoesTableExist("credit_cards")) {
    if (!db_->Execute("CREATE TABLE credit_cards ( "
                      "guid VARCHAR PRIMARY KEY, "
                      "name_on_card VARCHAR, "
                      "expiration_month INTEGER, "
                      "expiration_year INTEGER, "
                      "card_number_encrypted BLOB, "
                      "date_modified INTEGER NOT NULL DEFAULT 0, "
                      "origin VARCHAR DEFAULT '', "
                      "use_count INTEGER NOT NULL DEFAULT 0, "
                      "use_date INTEGER NOT NULL DEFAULT 0, "
                      "billing_address_id VARCHAR) ")) {
      NOTREACHED();
      return false;
    }
  }

  return true;
}

bool AutofillTable::InitProfilesTable() {
  if (!db_->DoesTableExist("autofill_profiles")) {
    if (!db_->Execute("CREATE TABLE autofill_profiles ( "
                      "guid VARCHAR PRIMARY KEY, "
                      "company_name VARCHAR, "
                      "street_address VARCHAR, "
                      "dependent_locality VARCHAR, "
                      "city VARCHAR, "
                      "state VARCHAR, "
                      "zipcode VARCHAR, "
                      "sorting_code VARCHAR, "
                      "country_code VARCHAR, "
                      "date_modified INTEGER NOT NULL DEFAULT 0, "
                      "origin VARCHAR DEFAULT '', "
                      "language_code VARCHAR, "
                      "use_count INTEGER NOT NULL DEFAULT 0, "
                      "use_date INTEGER NOT NULL DEFAULT 0, "
                      "validity_bitfield UNSIGNED NOT NULL DEFAULT 0, "
                      "is_client_validity_states_updated BOOL NOT NULL DEFAULT "
                      "FALSE) ")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitProfileNamesTable() {
  if (!db_->DoesTableExist("autofill_profile_names")) {
    if (!db_->Execute("CREATE TABLE autofill_profile_names ( "
                      "guid VARCHAR, "
                      "first_name VARCHAR, "
                      "middle_name VARCHAR, "
                      "last_name VARCHAR, "
                      "full_name VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitProfileEmailsTable() {
  if (!db_->DoesTableExist("autofill_profile_emails")) {
    if (!db_->Execute("CREATE TABLE autofill_profile_emails ( "
                      "guid VARCHAR, "
                      "email VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitProfilePhonesTable() {
  if (!db_->DoesTableExist("autofill_profile_phones")) {
    if (!db_->Execute("CREATE TABLE autofill_profile_phones ( "
                      "guid VARCHAR, "
                      "number VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitProfileTrashTable() {
  if (!db_->DoesTableExist("autofill_profiles_trash")) {
    if (!db_->Execute("CREATE TABLE autofill_profiles_trash ( "
                      "guid VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitMaskedCreditCardsTable() {
  if (!db_->DoesTableExist("masked_credit_cards")) {
    if (!db_->Execute("CREATE TABLE masked_credit_cards ("
                      "id VARCHAR,"
                      "status VARCHAR,"
                      "name_on_card VARCHAR,"
                      "network VARCHAR,"
                      "last_four VARCHAR,"
                      "exp_month INTEGER DEFAULT 0,"
                      "exp_year INTEGER DEFAULT 0, "
                      "bank_name VARCHAR, "
                      "type INTEGER DEFAULT 0)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitUnmaskedCreditCardsTable() {
  if (!db_->DoesTableExist("unmasked_credit_cards")) {
    if (!db_->Execute("CREATE TABLE unmasked_credit_cards ("
                      "id VARCHAR,"
                      "card_number_encrypted VARCHAR, "
                      "use_count INTEGER NOT NULL DEFAULT 0, "
                      "use_date INTEGER NOT NULL DEFAULT 0, "
                      "unmask_date INTEGER NOT NULL DEFAULT 0)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitServerCardMetadataTable() {
  if (!db_->DoesTableExist("server_card_metadata")) {
    if (!db_->Execute("CREATE TABLE server_card_metadata ("
                      "id VARCHAR NOT NULL,"
                      "use_count INTEGER NOT NULL DEFAULT 0, "
                      "use_date INTEGER NOT NULL DEFAULT 0, "
                      "billing_address_id VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitServerAddressesTable() {
  if (!db_->DoesTableExist("server_addresses")) {
    // The space after language_code is necessary to match what sqlite does
    // when it appends the column in migration.
    if (!db_->Execute("CREATE TABLE server_addresses ("
                      "id VARCHAR,"
                      "company_name VARCHAR,"
                      "street_address VARCHAR,"
                      "address_1 VARCHAR,"
                      "address_2 VARCHAR,"
                      "address_3 VARCHAR,"
                      "address_4 VARCHAR,"
                      "postal_code VARCHAR,"
                      "sorting_code VARCHAR,"
                      "country_code VARCHAR,"
                      "language_code VARCHAR, "   // Space required.
                      "recipient_name VARCHAR, "  // Ditto.
                      "phone_number VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitServerAddressMetadataTable() {
  if (!db_->DoesTableExist("server_address_metadata")) {
    if (!db_->Execute("CREATE TABLE server_address_metadata ("
                      "id VARCHAR NOT NULL,"
                      "use_count INTEGER NOT NULL DEFAULT 0, "
                      "use_date INTEGER NOT NULL DEFAULT 0, "
                      "has_converted BOOL NOT NULL DEFAULT FALSE)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitAutofillSyncMetadataTable() {
  if (!db_->DoesTableExist("autofill_sync_metadata")) {
    if (!db_->Execute("CREATE TABLE autofill_sync_metadata ("
                      "model_type INTEGER NOT NULL, "
                      "storage_key VARCHAR NOT NULL, "
                      "value BLOB, "
                      "PRIMARY KEY (model_type, storage_key))")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitModelTypeStateTable() {
  if (!db_->DoesTableExist("autofill_model_type_state")) {
    if (!db_->Execute("CREATE TABLE autofill_model_type_state ("
                      "model_type INTEGER NOT NULL PRIMARY KEY, value BLOB)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitPaymentsCustomerDataTable() {
  if (!db_->DoesTableExist("payments_customer_data")) {
    if (!db_->Execute("CREATE TABLE payments_customer_data "
                      "(customer_id VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AutofillTable::InitPaymentsUPIVPATable() {
  if (!db_->DoesTableExist("payments_upi_vpa")) {
    if (!db_->Execute("CREATE TABLE payments_upi_vpa ("
                      "vpa VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

}  // namespace autofill
