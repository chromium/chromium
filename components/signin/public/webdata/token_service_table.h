// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_WEBDATA_TOKEN_SERVICE_TABLE_H_
#define COMPONENTS_SIGNIN_PUBLIC_WEBDATA_TOKEN_SERVICE_TABLE_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
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

  struct TokenWithBindingKey {
    std::string token;
    std::vector<uint8_t> wrapped_binding_key;

    TokenWithBindingKey();
    explicit TokenWithBindingKey(std::string token,
                                 std::vector<uint8_t> wrapped_binding_key = {});

    TokenWithBindingKey(const TokenWithBindingKey&);
    TokenWithBindingKey& operator=(const TokenWithBindingKey&);

    ~TokenWithBindingKey();

    friend bool operator==(const TokenWithBindingKey&,
                           const TokenWithBindingKey&) = default;
  };

  TokenServiceTable();

  TokenServiceTable(const TokenServiceTable&) = delete;
  TokenServiceTable& operator=(const TokenServiceTable&) = delete;

  ~TokenServiceTable() override;

  // Retrieves the TokenServiceTable* owned by |database|.
  static TokenServiceTable* FromWebDatabase(WebDatabase* db);

  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Remove all tokens previously set with SetTokenForService.
  bool RemoveAllTokens();

  // Removes a token related to the service from the token_service table.
  bool RemoveTokenForService(const std::string& service);

  // Retrieves all tokens previously set with SetTokenForService.
  // Returns true if there were tokens and we decrypted them,
  // false if there was a failure somehow. If `should_reencrypt` is set to true,
  // then `SetTokenForService` should be called to write newly encrypted values
  // to storage.
  Result GetAllTokens(std::map<std::string, TokenWithBindingKey>* tokens,
                      bool& should_reencrypt);

  // Stores a token with an optional binding key in the token_service table.
  // Token is stored encrypted. May cause a mac keychain popup.
  // Returns true if we encrypted a token and stored it, false otherwise.
  bool SetTokenForService(const std::string& service,
                          const std::string& token,
                          const std::vector<uint8_t>& wrapped_binding_key);

 private:
  bool MigrateToVersion130AddBindingKeyColumn();
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_WEBDATA_TOKEN_SERVICE_TABLE_H_
