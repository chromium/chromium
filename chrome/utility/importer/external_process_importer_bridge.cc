// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/external_process_importer_bridge.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner.h"
#include "build/build_config.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_autofill_form_data_entry.h"
#include "chrome/common/importer/importer_data_types.h"
#include "components/autofill/core/common/password_form.h"

namespace {

// Rather than sending all import items over IPC at once we chunk them into
// separate requests.  This avoids the case of a large import causing
// oversized IPC messages.
const int kNumBookmarksToSend = 100;
const int kNumHistoryRowsToSend = 100;
const int kNumFaviconsToSend = 100;
const int kNumAutofillFormDataToSend = 100;

} // namespace

ExternalProcessImporterBridge::ExternalProcessImporterBridge(
    const base::flat_map<uint32_t, std::string>& localized_strings,
    mojo::SharedRemote<chrome::mojom::ProfileImportObserver> observer)
    : localized_strings_(std::move(localized_strings)),
      observer_(std::move(observer)) {}

void ExternalProcessImporterBridge::AddBookmarks(
    const std::vector<ImportedBookmarkEntry>& bookmarks,
    const base::string16& first_folder_name) {
  observer_->OnBookmarksImportStart(first_folder_name, bookmarks.size());

  // |bookmarks_left| is required for the checks below as Windows has a
  // Debug bounds-check which prevents pushing an iterator beyond its end()
  // (i.e., |it + 2 < s.end()| crashes in debug mode if |i + 1 == s.end()|).
  int bookmarks_left = bookmarks.end() - bookmarks.begin();
  for (auto it = bookmarks.begin(); it < bookmarks.end();) {
    std::vector<ImportedBookmarkEntry> bookmark_group;
    auto end_group = it + std::min(bookmarks_left, kNumBookmarksToSend);
    bookmark_group.assign(it, end_group);

    observer_->OnBookmarksImportGroup(bookmark_group);
    bookmarks_left -= end_group - it;
    it = end_group;
  }
  DCHECK_EQ(0, bookmarks_left);
}

void ExternalProcessImporterBridge::AddHomePage(const GURL& home_page) {
  observer_->OnHomePageImportReady(home_page);
}

void ExternalProcessImporterBridge::SetFavicons(
    const favicon_base::FaviconUsageDataList& favicons) {
  observer_->OnFaviconsImportStart(favicons.size());

  // |favicons_left| is required for the checks below as Windows has a
  // Debug bounds-check which prevents pushing an iterator beyond its end()
  // (i.e., |it + 2 < s.end()| crashes in debug mode if |i + 1 == s.end()|).
  int favicons_left = favicons.end() - favicons.begin();
  for (auto it = favicons.begin(); it < favicons.end();) {
    favicon_base::FaviconUsageDataList favicons_group;
    auto end_group = it + std::min(favicons_left, kNumFaviconsToSend);
    favicons_group.assign(it, end_group);

    observer_->OnFaviconsImportGroup(favicons_group);
    favicons_left -= end_group - it;
    it = end_group;
  }
  DCHECK_EQ(0, favicons_left);
}

void ExternalProcessImporterBridge::SetHistoryItems(
    const std::vector<ImporterURLRow>& rows,
    importer::VisitSource visit_source) {
  observer_->OnHistoryImportStart(rows.size());

  // |rows_left| is required for the checks below as Windows has a
  // Debug bounds-check which prevents pushing an iterator beyond its end()
  // (i.e., |it + 2 < s.end()| crashes in debug mode if |i + 1 == s.end()|).
  int rows_left = rows.end() - rows.begin();
  for (auto it = rows.begin(); it < rows.end();) {
    std::vector<ImporterURLRow> row_group;
    auto end_group = it + std::min(rows_left, kNumHistoryRowsToSend);
    row_group.assign(it, end_group);

    observer_->OnHistoryImportGroup(row_group, visit_source);
    rows_left -= end_group - it;
    it = end_group;
  }
  DCHECK_EQ(0, rows_left);
}

void ExternalProcessImporterBridge::SetKeywords(
    const std::vector<importer::SearchEngineInfo>& search_engines,
    bool unique_on_host_and_path) {
  observer_->OnKeywordsImportReady(search_engines, unique_on_host_and_path);
}

void ExternalProcessImporterBridge::SetFirefoxSearchEnginesXMLData(
    const std::vector<std::string>& search_engine_data) {
  observer_->OnFirefoxSearchEngineDataReceived(search_engine_data);
}

void ExternalProcessImporterBridge::SetPasswordForm(
    const autofill::PasswordForm& form) {
  observer_->OnPasswordFormImportReady(form);
}

void ExternalProcessImporterBridge::SetAutofillFormData(
    const std::vector<ImporterAutofillFormDataEntry>& entries) {
  observer_->OnAutofillFormDataImportStart(entries.size());

  // |autofill_form_data_entries_left| is required for the checks below as
  // Windows has a Debug bounds-check which prevents pushing an iterator beyond
  // its end() (i.e., |it + 2 < s.end()| crashes in debug mode if |i + 1 ==
  // s.end()|).
  int autofill_form_data_entries_left = entries.end() - entries.begin();
  for (auto it = entries.begin(); it < entries.end();) {
    std::vector<ImporterAutofillFormDataEntry> autofill_form_data_entry_group;
    auto end_group = it + std::min(autofill_form_data_entries_left,
                                   kNumAutofillFormDataToSend);
    autofill_form_data_entry_group.assign(it, end_group);

    observer_->OnAutofillFormDataImportGroup(autofill_form_data_entry_group);
    autofill_form_data_entries_left -= end_group - it;
    it = end_group;
  }
  DCHECK_EQ(0, autofill_form_data_entries_left);
}

void ExternalProcessImporterBridge::NotifyStarted() {
  observer_->OnImportStart();
}

void ExternalProcessImporterBridge::NotifyItemStarted(
    importer::ImportItem item) {
  observer_->OnImportItemStart(item);
}

void ExternalProcessImporterBridge::NotifyItemEnded(importer::ImportItem item) {
  observer_->OnImportItemFinished(item);
}

void ExternalProcessImporterBridge::NotifyEnded() {
  observer_->OnImportFinished(true, std::string());
}

base::string16 ExternalProcessImporterBridge::GetLocalizedString(
    int message_id) {
  DCHECK(localized_strings_.count(message_id));
  return base::UTF8ToUTF16(localized_strings_[message_id]);
}

ExternalProcessImporterBridge::~ExternalProcessImporterBridge() {}
