// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/webdata/token_web_data.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/task/sequenced_task_runner.h"
#include "components/signin/public/webdata/token_service_table.h"
#include "components/webdata/common/web_database_service.h"

using base::BindOnce;
using base::Time;

class TokenWebDataBackend
    : public base::RefCountedDeleteOnSequence<TokenWebDataBackend> {
 public:
  explicit TokenWebDataBackend(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner)
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

  WebDatabase::State SetTokenForService(
      const std::string& service,
      const std::string& token,
      const std::vector<uint8_t>& wrapped_binding_key,
      WebDatabase* db) {
    if (TokenServiceTable::FromWebDatabase(db)->SetTokenForService(
            service, token, wrapped_binding_key)) {
      return WebDatabase::COMMIT_NEEDED;
    }
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  std::unique_ptr<WDTypedResult> GetAllTokens(WebDatabase* db) {
    TokenResult result;
    result.db_result = TokenServiceTable::FromWebDatabase(db)->GetAllTokens(
        &result.tokens, result.should_reencrypt);
    return std::make_unique<WDResult<TokenResult>>(TOKEN_RESULT, result);
  }

 protected:
  virtual ~TokenWebDataBackend() = default;

 private:
  friend class base::RefCountedDeleteOnSequence<TokenWebDataBackend>;
  friend class base::DeleteHelper<TokenWebDataBackend>;
};

TokenResult::TokenResult() = default;
TokenResult::TokenResult(const TokenResult& other) = default;
TokenResult& TokenResult::operator=(const TokenResult& other) = default;
TokenResult::~TokenResult() = default;

TokenWebData::TokenWebData(
    scoped_refptr<WebDatabaseService> wdbs,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : WebDataServiceBase(wdbs, std::move(ui_task_runner)),
      token_backend_(new TokenWebDataBackend(wdbs->GetDbSequence())) {}

void TokenWebData::SetTokenForService(
    const std::string& service,
    const std::string& token,
    const std::vector<uint8_t>& wrapped_binding_key) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, BindOnce(&TokenWebDataBackend::SetTokenForService,
                          token_backend_, service, token, wrapped_binding_key));
}

void TokenWebData::RemoveAllTokens() {
  wdbs_->ScheduleDBTask(
      FROM_HERE,
      BindOnce(&TokenWebDataBackend::RemoveAllTokens, token_backend_));
}

void TokenWebData::RemoveTokenForService(const std::string& service) {
  wdbs_->ScheduleDBTask(FROM_HERE,
                        BindOnce(&TokenWebDataBackend::RemoveTokenForService,
                                 token_backend_, service));
}

// Null on failure. Success is WDResult<std::string>
WebDataServiceBase::Handle TokenWebData::GetAllTokens(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE, BindOnce(&TokenWebDataBackend::GetAllTokens, token_backend_),
      consumer);
}

TokenWebData::~TokenWebData() = default;
