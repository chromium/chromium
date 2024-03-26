// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/in_memory_history_backend.h"

#include <memory>
#include <set>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/history/core/browser/in_memory_database.h"
#include "components/history/core/browser/url_database.h"

namespace history {

InMemoryHistoryBackend::InMemoryHistoryBackend() = default;
InMemoryHistoryBackend::~InMemoryHistoryBackend() = default;

bool InMemoryHistoryBackend::Init(const base::FilePath& history_filename) {
  db_ = std::make_unique<InMemoryDatabase>();
  return db_->InitFromDisk(history_filename);
}

void InMemoryHistoryBackend::AttachToHistoryService(
    HistoryService* history_service) {
  DCHECK(db_);
  DCHECK(history_service);
  history_service_observation_.Observe(history_service);
}

void InMemoryHistoryBackend::DeleteAllSearchTermsForKeyword(
    KeywordID keyword_id) {
  // For simplicity, this will not remove the corresponding URLRows, but
  // this is okay, as the main database does not do so either.
  db_->DeleteAllSearchTermsForKeyword(keyword_id);
}

void InMemoryHistoryBackend::OnURLVisited(
    history::HistoryService* history_service,
    const history::URLRow& url_row,
    const history::VisitRow& new_visit) {
  OnURLVisitedOrModified(url_row);
}

void InMemoryHistoryBackend::OnURLsModified(HistoryService* history_service,
                                            const URLRows& changed_urls) {
  for (const auto& row : changed_urls) {
    OnURLVisitedOrModified(row);
  }
}

void InMemoryHistoryBackend::OnHistoryDeletions(
    HistoryService* history_service,
    const DeletionInfo& deletion_info) {
  DCHECK(db_);

  if (deletion_info.IsAllHistory()) {
    // When all history is deleted, the individual URLs won't be listed. Just
    // create a new database to quickly clear everything out.
    db_ = std::make_unique<InMemoryDatabase>();
    if (!db_->InitFromScratch())
      db_.reset();
    return;
  }

  // Delete all matching URLs in our database.
  for (const auto& row : deletion_info.deleted_rows()) {
    // This will also delete the corresponding keyword search term.
    // Ignore errors, as we typically only cache a subset of URLRows.
    db_->DeleteURLRow(row.id());
  }
}

void InMemoryHistoryBackend::OnKeywordSearchTermUpdated(
    HistoryService* history_service,
    const URLRow& row,
    KeywordID keyword_id,
    const std::u16string& term) {
  DCHECK(row.id());
  db_->InsertOrUpdateURLRowByID(row);
  db_->SetKeywordSearchTermsForURL(row.id(), keyword_id, term);
}

void InMemoryHistoryBackend::OnKeywordSearchTermDeleted(
    HistoryService* history_service,
    URLID url_id) {
  // For simplicity, this will not remove the corresponding URLRow, but this is
  // okay, as the main database does not do so either.
  db_->DeleteKeywordSearchTermForURL(url_id);
}

void InMemoryHistoryBackend::OnURLVisitedOrModified(const URLRow& url_row) {
  DCHECK(db_);
  DCHECK(url_row.id());
  if (url_row.typed_count() ||
      db_->GetKeywordSearchTermRow(url_row.id(), nullptr))
    db_->InsertOrUpdateURLRowByID(url_row);
  else
    db_->DeleteURLRow(url_row.id());
}

}  // namespace history
