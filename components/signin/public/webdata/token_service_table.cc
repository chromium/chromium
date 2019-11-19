// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/webdata/token_service_table.h"

#include <map>
#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/os_crypt/os_crypt.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"

namespace {

WebDatabaseTable::TypeKey GetKey() {
  // We just need a unique constant. Use the address of a static that
  // COMDAT folding won't touch in an optimizing linker.
  static int table_key = 0;
  return reinterpret_cast<void*>(&table_key);
}

// Entries in the |Signin.TokenTable.ReadTokenFromDBResult| histogram.
enum ReadOneTokenResult {
  READ_ONE_TOKEN_SUCCESS,
  READ_ONE_TOKEN_DB_SUCCESS_DECRYPT_FAILED,
  READ_ONE_TOKEN_DB_FAILED_BAD_ENTRY,
  READ_ONE_TOKEN_MAX_VALUE
};

}  // namespace

TokenServiceTable* TokenServiceTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<TokenServiceTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey TokenServiceTable::GetTypeKey() const {
  return GetKey();
}

bool TokenServiceTable::CreateTablesIfNecessary() {
  if (!db_->DoesTableExist("token_service")) {
    if (!db_->Execute("CREATE TABLE token_service ("
                      "service VARCHAR PRIMARY KEY NOT NULL,"
                      "encrypted_token BLOB)")) {
      NOTREACHED();
      LOG(ERROR) << "Failed creating token_service table";
      return false;
    }
  }
  return true;
}

bool TokenServiceTable::IsSyncable() {
  return true;
}

bool TokenServiceTable::MigrateToVersion(int version,
                                         bool* update_compatible_version) {
  return true;
}

bool TokenServiceTable::RemoveAllTokens() {
  VLOG(1) << "Remove all tokens";
  sql::Statement s(db_->GetUniqueStatement("DELETE FROM token_service"));

  bool result = s.Run();
  LOG_IF(ERROR, !result) << "Failed to remove all tokens";
  return result;
}

bool TokenServiceTable::RemoveTokenForService(const std::string& service) {
  sql::Statement s(
      db_->GetUniqueStatement("DELETE FROM token_service WHERE service = ?"));
  s.BindString(0, service);

  bool result = s.Run();
  LOG_IF(ERROR, !result) << "Failed to remove token for " << service;
  return result;
}

bool TokenServiceTable::SetTokenForService(const std::string& service,
                                           const std::string& token) {
  std::string encrypted_token;
  bool encrypted = OSCrypt::EncryptString(token, &encrypted_token);
  if (!encrypted) {
    LOG(ERROR) << "Failed to encrypt token (token will not be saved to DB).";
    return false;
  }

  // Don't bother with a cached statement since this will be a relatively
  // infrequent operation.
  sql::Statement s(
      db_->GetUniqueStatement("INSERT OR REPLACE INTO token_service "
                              "(service, encrypted_token) VALUES (?, ?)"));
  s.BindString(0, service);
  s.BindBlob(1, encrypted_token.data(),
             static_cast<int>(encrypted_token.length()));

  bool result = s.Run();
  LOG_IF(ERROR, !result) << "Failed to insert or replace token for " << service;
  return result;
}

TokenServiceTable::Result TokenServiceTable::GetAllTokens(
    std::map<std::string, std::string>* tokens) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT service, encrypted_token FROM token_service"));

  UMA_HISTOGRAM_BOOLEAN("Signin.TokenTable.GetAllTokensSqlStatementValidity",
                        s.is_valid());

  if (!s.is_valid()) {
    LOG(ERROR) << "Failed to load tokens (invalid SQL statement).";
    return TOKEN_DB_RESULT_SQL_INVALID_STATEMENT;
  }

  int number_of_tokens_loaded = 0;

  Result read_all_tokens_result = TOKEN_DB_RESULT_SUCCESS;
  while (s.Step()) {
    ReadOneTokenResult read_token_result = READ_ONE_TOKEN_MAX_VALUE;

    std::string encrypted_token;
    std::string decrypted_token;
    std::string service;
    service = s.ColumnString(0);
    bool entry_ok =
        !service.empty() && s.ColumnBlobAsString(1, &encrypted_token);
    if (entry_ok) {
      if (OSCrypt::DecryptString(encrypted_token, &decrypted_token)) {
        (*tokens)[service] = decrypted_token;
        read_token_result = READ_ONE_TOKEN_SUCCESS;
        number_of_tokens_loaded++;
      } else {
        // Chrome relies on native APIs to encrypt and decrypt the tokens which
        // may fail (see http://crbug.com/686485).
        LOG(ERROR) << "Failed to decrypt token for service " << service;
        read_token_result = READ_ONE_TOKEN_DB_SUCCESS_DECRYPT_FAILED;
        read_all_tokens_result = TOKEN_DB_RESULT_DECRYPT_ERROR;
      }
    } else {
      LOG(ERROR) << "Bad token entry for service " << service;
      read_token_result = READ_ONE_TOKEN_DB_FAILED_BAD_ENTRY;
      read_all_tokens_result = TOKEN_DB_RESULT_BAD_ENTRY;
    }
    DCHECK_LT(read_token_result, READ_ONE_TOKEN_MAX_VALUE);
    UMA_HISTOGRAM_ENUMERATION("Signin.TokenTable.ReadTokenFromDBResult",
                              read_token_result, READ_ONE_TOKEN_MAX_VALUE);
  }
  VLOG(1) << "Loaded tokens: result = " << read_all_tokens_result
          << " ; number of tokens loaded = " << number_of_tokens_loaded;
  return read_all_tokens_result;
}
