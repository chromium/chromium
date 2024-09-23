// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef COMPONENTS_SIGNIN_PUBLIC_WEBDATA_TOKEN_WEB_DATA_H_
#define COMPONENTS_SIGNIN_PUBLIC_WEBDATA_TOKEN_WEB_DATA_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/signin/public/webdata/token_service_table.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database.h"

namespace base {
class SequencedTaskRunner;
}

class TokenWebDataBackend;
class WebDatabaseService;
class WebDataServiceConsumer;

// The result of a get tokens operation.
struct TokenResult {
  TokenResult();
  TokenResult(const TokenResult& other);
  TokenResult& operator=(const TokenResult& other);
  ~TokenResult();

  TokenServiceTable::Result db_result =
      TokenServiceTable::TOKEN_DB_RESULT_SQL_INVALID_STATEMENT;
  std::map<std::string, TokenServiceTable::TokenWithBindingKey> tokens;
  bool should_reencrypt = false;
};

// TokenWebData is a data repository for storage of authentication tokens.

class TokenWebData : public WebDataServiceBase {
 public:
  TokenWebData(scoped_refptr<WebDatabaseService> wdbs,
               scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  TokenWebData(const TokenWebData&) = delete;
  TokenWebData& operator=(const TokenWebData&) = delete;

  // Set a token to use for a specified service.
  void SetTokenForService(const std::string& service,
                          const std::string& token,
                          const std::vector<uint8_t>& wrapped_binding_key);

  // Remove all tokens stored in the web database.
  void RemoveAllTokens();

  // Removes a token related to |service| from the web database.
  void RemoveTokenForService(const std::string& service);

  // Null on failure. Success is WDResult<std::vector<std::string> >
  virtual Handle GetAllTokens(WebDataServiceConsumer* consumer);

 protected:
  // For unit tests, passes a null callback.
  TokenWebData();

  ~TokenWebData() override;

 private:
  scoped_refptr<TokenWebDataBackend> token_backend_;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_WEBDATA_TOKEN_WEB_DATA_H_
