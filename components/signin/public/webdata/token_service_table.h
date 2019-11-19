// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_WEBDATA_TOKEN_SERVICE_TABLE_H_
#define COMPONENTS_SIGNIN_PUBLIC_WEBDATA_TOKEN_SERVICE_TABLE_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

class TokenServiceTable : public WebDatabaseTable {
 public:
  enum Result {
    TOKEN_DB_RESULT_SQL_INVALID_STATEMENT,
    TOKEN_DB_RESULT_BAD_ENTRY,
    TOKEN_DB_RESULT_DECRYPT_ERROR,
    TOKEN_DB_RESULT_SUCCESS
  };

  TokenServiceTable() {}
  ~TokenServiceTable() override {}

  // Retrieves the TokenServiceTable* owned by |database|.
  static TokenServiceTable* FromWebDatabase(WebDatabase* db);

  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool IsSyncable() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Remove all tokens previously set with SetTokenForService.
  bool RemoveAllTokens();

  // Removes a token related to the service from the token_service table.
  bool RemoveTokenForService(const std::string& service);

  // Retrieves all tokens previously set with SetTokenForService.
  // Returns true if there were tokens and we decrypted them,
  // false if there was a failure somehow
  Result GetAllTokens(std::map<std::string, std::string>* tokens);

  // Store a token in the token_service table. Stored encrypted. May cause
  // a mac keychain popup.
  // True if we encrypted a token and stored it, false otherwise.
  bool SetTokenForService(const std::string& service, const std::string& token);

 private:
  DISALLOW_COPY_AND_ASSIGN(TokenServiceTable);
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_WEBDATA_TOKEN_SERVICE_TABLE_H_
