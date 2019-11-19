// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/webdata/token_web_data.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "components/signin/public/webdata/token_service_table.h"
#include "components/webdata/common/web_database_service.h"

using base::Bind;
using base::Time;

class TokenWebDataBackend
    : public base::RefCountedDeleteOnSequence<TokenWebDataBackend> {
 public:
  TokenWebDataBackend(
      scoped_refptr<base::SingleThreadTaskRunner> db_task_runner)
      : base::RefCountedDeleteOnSequence<TokenWebDataBackend>(db_task_runner) {}

  WebDatabase::State RemoveAllTokens(WebDatabase* db) {
    if (TokenServiceTable::FromWebDatabase(db)->RemoveAllTokens()) {
      return WebDatabase::COMMIT_NEEDED;
    }
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  WebDatabase::State RemoveTokenForService(const std::string& service,
                                           WebDatabase* db) {
    if (TokenServiceTable::FromWebDatabase(db)->RemoveTokenForService(
            service)) {
      return WebDatabase::COMMIT_NEEDED;
    }
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  WebDatabase::State SetTokenForService(const std::string& service,
                                        const std::string& token,
                                        WebDatabase* db) {
    if (TokenServiceTable::FromWebDatabase(db)->SetTokenForService(service,
                                                                   token)) {
      return WebDatabase::COMMIT_NEEDED;
    }
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  std::unique_ptr<WDTypedResult> GetAllTokens(WebDatabase* db) {
    TokenResult result;
    result.db_result =
        TokenServiceTable::FromWebDatabase(db)->GetAllTokens(&result.tokens);
    return std::make_unique<WDResult<TokenResult>>(TOKEN_RESULT, result);
  }

 protected:
  virtual ~TokenWebDataBackend() {}

 private:
  friend class base::RefCountedDeleteOnSequence<TokenWebDataBackend>;
  friend class base::DeleteHelper<TokenWebDataBackend>;
};

TokenResult::TokenResult()
    : db_result(TokenServiceTable::TOKEN_DB_RESULT_SQL_INVALID_STATEMENT) {}
TokenResult::TokenResult(const TokenResult& other) = default;
TokenResult::~TokenResult() {}

TokenWebData::TokenWebData(
    scoped_refptr<WebDatabaseService> wdbs,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_task_runner,
    const ProfileErrorCallback& callback)
    : WebDataServiceBase(wdbs, callback, ui_task_runner),
      token_backend_(new TokenWebDataBackend(db_task_runner)) {}

void TokenWebData::SetTokenForService(const std::string& service,
                                      const std::string& token) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, Bind(&TokenWebDataBackend::SetTokenForService, token_backend_,
                      service, token));
}

void TokenWebData::RemoveAllTokens() {
  wdbs_->ScheduleDBTask(
      FROM_HERE, Bind(&TokenWebDataBackend::RemoveAllTokens, token_backend_));
}

void TokenWebData::RemoveTokenForService(const std::string& service) {
  wdbs_->ScheduleDBTask(FROM_HERE,
                        Bind(&TokenWebDataBackend::RemoveTokenForService,
                             token_backend_, service));
}

// Null on failure. Success is WDResult<std::string>
WebDataServiceBase::Handle TokenWebData::GetAllTokens(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE, Bind(&TokenWebDataBackend::GetAllTokens, token_backend_),
      consumer);
}

TokenWebData::TokenWebData(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_task_runner)
    : WebDataServiceBase(nullptr, ProfileErrorCallback(), ui_task_runner),
      token_backend_(new TokenWebDataBackend(db_task_runner)) {}

TokenWebData::~TokenWebData() {}
