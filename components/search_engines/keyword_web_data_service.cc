// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/keyword_web_data_service.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/template_url_data.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_database_service.h"

namespace {

WebDatabase::State PerformKeywordOperationsImpl(
    const KeywordTable::Operations& operations,
    WebDatabase* db) {
  return KeywordTable::FromWebDatabase(db)->PerformOperations(operations)
             ? WebDatabase::COMMIT_NEEDED
             : WebDatabase::COMMIT_NOT_NEEDED;
}

std::unique_ptr<WDTypedResult> GetKeywordsImpl(WebDatabase* db) {
  KeywordTable* const keyword_table = KeywordTable::FromWebDatabase(db);
  WDKeywordsResult result;
  if (!keyword_table->GetKeywords(&result.keywords))
    return nullptr;

  result.default_search_provider_id =
      keyword_table->GetDefaultSearchProviderID();
  result.builtin_keyword_version = keyword_table->GetBuiltinKeywordVersion();
  return std::make_unique<WDResult<WDKeywordsResult>>(KEYWORDS_RESULT, result);
}

WebDatabase::State SetDefaultSearchProviderIDImpl(TemplateURLID id,
                                                  WebDatabase* db) {
  return KeywordTable::FromWebDatabase(db)->SetDefaultSearchProviderID(id)
             ? WebDatabase::COMMIT_NEEDED
             : WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State SetBuiltinKeywordVersionImpl(int version, WebDatabase* db) {
  return KeywordTable::FromWebDatabase(db)->SetBuiltinKeywordVersion(version)
             ? WebDatabase::COMMIT_NEEDED
             : WebDatabase::COMMIT_NOT_NEEDED;
}

}  // namespace

WDKeywordsResult::WDKeywordsResult() = default;

WDKeywordsResult::WDKeywordsResult(const WDKeywordsResult&) = default;

WDKeywordsResult& WDKeywordsResult::operator=(const WDKeywordsResult&) =
    default;

WDKeywordsResult::~WDKeywordsResult() = default;

KeywordWebDataService::BatchModeScoper::BatchModeScoper(
    KeywordWebDataService* service)
    : service_(service) {
  if (service_)
    service_->AdjustBatchModeLevel(true);
}

KeywordWebDataService::BatchModeScoper::~BatchModeScoper() {
  if (service_)
    service_->AdjustBatchModeLevel(false);
}

KeywordWebDataService::KeywordWebDataService(
    scoped_refptr<WebDatabaseService> wdbs,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const ProfileErrorCallback& callback)
    : WebDataServiceBase(wdbs, callback, ui_task_runner),
      timer_(FROM_HERE,
             base::TimeDelta::FromSeconds(5),
             base::BindRepeating(&KeywordWebDataService::CommitQueuedOperations,
                                 base::Unretained(this))) {}

void KeywordWebDataService::AddKeyword(const TemplateURLData& data) {
  if (batch_mode_level_) {
    queued_keyword_operations_.push_back(
        KeywordTable::Operation(KeywordTable::ADD, data));
  } else {
    AdjustBatchModeLevel(true);
    AddKeyword(data);
    AdjustBatchModeLevel(false);
  }
}

void KeywordWebDataService::RemoveKeyword(TemplateURLID id) {
  if (batch_mode_level_) {
    TemplateURLData data;
    data.id = id;
    queued_keyword_operations_.push_back(
        KeywordTable::Operation(KeywordTable::REMOVE, data));
  } else {
    AdjustBatchModeLevel(true);
    RemoveKeyword(id);
    AdjustBatchModeLevel(false);
  }
}

void KeywordWebDataService::UpdateKeyword(const TemplateURLData& data) {
  if (batch_mode_level_) {
    queued_keyword_operations_.push_back(
        KeywordTable::Operation(KeywordTable::UPDATE, data));
  } else {
    AdjustBatchModeLevel(true);
    UpdateKeyword(data);
    AdjustBatchModeLevel(false);
  }
}

WebDataServiceBase::Handle KeywordWebDataService::GetKeywords(
    WebDataServiceConsumer* consumer) {
  // Force pending changes to be visible immediately so the results of this call
  // won't be out of date.
  CommitQueuedOperations();

  return wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE, base::Bind(&GetKeywordsImpl), consumer);
}

void KeywordWebDataService::SetDefaultSearchProviderID(TemplateURLID id) {
  wdbs_->ScheduleDBTask(FROM_HERE,
                        base::Bind(&SetDefaultSearchProviderIDImpl, id));
}

void KeywordWebDataService::SetBuiltinKeywordVersion(int version) {
  wdbs_->ScheduleDBTask(FROM_HERE,
                        base::Bind(&SetBuiltinKeywordVersionImpl, version));
}

void KeywordWebDataService::ShutdownOnUISequence() {
  CommitQueuedOperations();
  WebDataServiceBase::ShutdownOnUISequence();
}

KeywordWebDataService::~KeywordWebDataService() {
  DCHECK(!batch_mode_level_);
  DCHECK(queued_keyword_operations_.empty());
}

void KeywordWebDataService::AdjustBatchModeLevel(bool entering_batch_mode) {
  if (entering_batch_mode) {
    ++batch_mode_level_;
  } else {
    DCHECK(batch_mode_level_);
    --batch_mode_level_;
    if (!batch_mode_level_ && !queued_keyword_operations_.empty() &&
        !timer_.IsRunning()) {
      // When killing an app on Android/iOS, shutdown isn't guaranteed to be
      // called. Finishing this task immediately ensures the table is fully
      // populated even if the app is killed before the timer expires.
#if defined(OS_ANDROID) || defined(OS_IOS)
      CommitQueuedOperations();
#else
      timer_.Reset();
#endif
    }
  }
}

void KeywordWebDataService::CommitQueuedOperations() {
  if (!queued_keyword_operations_.empty()) {
    wdbs_->ScheduleDBTask(FROM_HERE, base::Bind(&PerformKeywordOperationsImpl,
                                                queued_keyword_operations_));
    queued_keyword_operations_.clear();
  }
  timer_.Stop();
}
