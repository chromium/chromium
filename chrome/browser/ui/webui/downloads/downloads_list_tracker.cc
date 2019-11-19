// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads/downloads_list_tracker.h"

#include <iterator>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/i18n/unicodestring.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_query.h"
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "extensions/browser/extension_registry.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/filename_util.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "ui/base/l10n/time_format.h"

using content::BrowserContext;
using download::DownloadItem;
using content::DownloadManager;

using DownloadVector = DownloadManager::DownloadVector;

namespace {

// Returns a string constant to be used as the |danger_type| value in
// CreateDownloadData(). This can be the empty string, if the danger type is not
// relevant for the UI.
const char* GetDangerTypeString(download::DownloadDangerType danger_type) {
  switch (danger_type) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return "DANGEROUS_FILE";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return "DANGEROUS_URL";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return "DANGEROUS_CONTENT";
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return "UNCOMMON_CONTENT";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return "DANGEROUS_HOST";
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return "POTENTIALLY_UNWANTED";
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
      return "ASYNC_SCANNING";
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
      return "BLOCKED_PASSWORD_PROTECTED";
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
      return "BLOCKED_TOO_LARGE";
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      return "SENSITIVE_CONTENT_WARNING";
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
      return "SENSITIVE_CONTENT_BLOCK";
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
      return "DEEP_SCANNED_SAFE";
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
      return "DEEP_SCANNED_OPENED_DANGEROUS";
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_WHITELISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      break;
  }

  // Don't return a danger type string if it is NOT_DANGEROUS,
  // MAYBE_DANGEROUS_CONTENT, or USER_VALIDATED, or WHITELISTED_BY_POLICY.
  return "";
}

// TODO(dbeam): if useful elsewhere, move to base/i18n/time_formatting.h?
std::string TimeFormatLongDate(const base::Time& time) {
  std::unique_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kLong));
  icu::UnicodeString date_string;
  formatter->format(static_cast<UDate>(time.ToDoubleT() * 1000), date_string);
  return base::UTF16ToUTF8(base::i18n::UnicodeStringToString16(date_string));
}

}  // namespace

DownloadsListTracker::DownloadsListTracker(
    DownloadManager* download_manager,
    mojo::PendingRemote<downloads::mojom::Page> page)
    : main_notifier_(download_manager, this),
      page_(std::move(page)),
      should_show_(base::BindRepeating(&DownloadsListTracker::ShouldShow,
                                       base::Unretained(this))) {
  Init();
}

DownloadsListTracker::~DownloadsListTracker() {}

void DownloadsListTracker::Reset() {
  if (sending_updates_)
    page_->ClearAll();
  sent_to_page_ = 0u;
}

bool DownloadsListTracker::SetSearchTerms(
    const std::vector<std::string>& search_terms) {
  std::vector<base::string16> new_terms;
  new_terms.resize(search_terms.size());

  for (const auto& t : search_terms)
    new_terms.push_back(base::UTF8ToUTF16(t));

  if (new_terms == search_terms_)
    return false;

  search_terms_.swap(new_terms);
  RebuildSortedItems();
  return true;
}

void DownloadsListTracker::StartAndSendChunk() {
  sending_updates_ = true;

  CHECK_LE(sent_to_page_, sorted_items_.size());

  auto it = sorted_items_.begin();
  std::advance(it, sent_to_page_);

  std::vector<downloads::mojom::DataPtr> list;
  while (it != sorted_items_.end() && list.size() < chunk_size_) {
    list.push_back(CreateDownloadData(*it));
    ++it;
  }

  size_t list_size = list.size();
  page_->InsertItems(static_cast<int>(sent_to_page_), std::move(list));

  sent_to_page_ += list_size;
}

void DownloadsListTracker::Stop() {
  sending_updates_ = false;
}

DownloadManager* DownloadsListTracker::GetMainNotifierManager() const {
  return main_notifier_.GetManager();
}

DownloadManager* DownloadsListTracker::GetOriginalNotifierManager() const {
  return original_notifier_ ? original_notifier_->GetManager() : nullptr;
}

void DownloadsListTracker::OnDownloadCreated(DownloadManager* manager,
                                             DownloadItem* download_item) {
  DCHECK_EQ(0u, sorted_items_.count(download_item));
  if (should_show_.Run(*download_item))
    InsertItem(sorted_items_.insert(download_item).first);
}

void DownloadsListTracker::OnDownloadUpdated(DownloadManager* manager,
                                             DownloadItem* download_item) {
  auto current_position = sorted_items_.find(download_item);
  bool is_showing = current_position != sorted_items_.end();
  bool should_show = should_show_.Run(*download_item);

  if (!is_showing && should_show)
    InsertItem(sorted_items_.insert(download_item).first);
  else if (is_showing && !should_show)
    RemoveItem(current_position);
  else if (is_showing)
    UpdateItem(current_position);
}

void DownloadsListTracker::OnDownloadRemoved(DownloadManager* manager,
                                             DownloadItem* download_item) {
  auto current_position = sorted_items_.find(download_item);
  if (current_position != sorted_items_.end())
    RemoveItem(current_position);
}

DownloadsListTracker::DownloadsListTracker(
    DownloadManager* download_manager,
    mojo::PendingRemote<downloads::mojom::Page> page,
    base::Callback<bool(const DownloadItem&)> should_show)
    : main_notifier_(download_manager, this),
      page_(std::move(page)),
      should_show_(should_show) {
  DCHECK(page_);
  Init();
}

downloads::mojom::DataPtr DownloadsListTracker::CreateDownloadData(
    download::DownloadItem* download_item) const {
  // TODO(asanka): Move towards using download_model here for getting status and
  // progress. The difference currently only matters to Drive downloads and
  // those don't show up on the downloads page, but should.
  DownloadItemModel download_model(download_item);

  auto file_value = downloads::mojom::Data::New();

  file_value->started =
      static_cast<int>(download_item->GetStartTime().ToTimeT());
  file_value->since_string = base::UTF16ToUTF8(
      ui::TimeFormat::RelativeDate(download_item->GetStartTime(), NULL));
  file_value->date_string = TimeFormatLongDate(download_item->GetStartTime());

  file_value->id = base::NumberToString(download_item->GetId());

  base::FilePath download_path(download_item->GetTargetFilePath());
  file_value->file_path = download_path.AsUTF8Unsafe();
  file_value->file_url = net::FilePathToFileURL(download_path).spec();

  extensions::DownloadedByExtension* by_ext =
      extensions::DownloadedByExtension::Get(download_item);
  std::string by_ext_id;
  std::string by_ext_name;
  if (by_ext) {
    by_ext_id = by_ext->id();
    // TODO(dbeam): why doesn't DownloadsByExtension::name() return a string16?
    by_ext_name = by_ext->name();

    // Lookup the extension's current name() in case the user changed their
    // language. This won't work if the extension was uninstalled, so the name
    // might be the wrong language.
    auto* profile = Profile::FromBrowserContext(
        content::DownloadItemUtils::GetBrowserContext(download_item));
    auto* registry = extensions::ExtensionRegistry::Get(profile);
    const extensions::Extension* extension = registry->GetExtensionById(
        by_ext->id(), extensions::ExtensionRegistry::EVERYTHING);
    if (extension)
      by_ext_name = extension->name();
  }
  file_value->by_ext_id = by_ext_id;
  file_value->by_ext_name = by_ext_name;

  // Keep file names as LTR. TODO(dbeam): why?
  base::string16 file_name =
      download_item->GetFileNameToReportUser().LossyDisplayName();
  file_name = base::i18n::GetDisplayStringInLTRDirectionality(file_name);
  file_value->file_name = base::UTF16ToUTF8(file_name);
  file_value->url = download_item->GetURL().spec();
  file_value->total = static_cast<int>(download_item->GetTotalBytes());
  file_value->file_externally_removed =
      download_item->GetFileExternallyRemoved();
  file_value->resume = download_item->CanResume();
  file_value->otr = IsIncognito(*download_item);

  const char* danger_type = GetDangerTypeString(download_item->GetDangerType());
  base::string16 last_reason_text;
  // -2 is invalid, -1 means indeterminate, and 0-100 are in-progress.
  int percent = -2;
  base::string16 progress_status_text;
  bool retry = false;
  const char* state = nullptr;

  switch (download_item->GetState()) {
    case download::DownloadItem::IN_PROGRESS: {
      if (download_item->IsDangerous()) {
        state = "DANGEROUS";
      } else if (download_item->IsPaused()) {
        state = "PAUSED";
      } else {
        state = "IN_PROGRESS";
      }
      progress_status_text = download_model.GetTabProgressStatusText();
      percent = download_item->PercentComplete();
      break;
    }

    case download::DownloadItem::INTERRUPTED:
      state = "INTERRUPTED";
      progress_status_text = download_model.GetTabProgressStatusText();

      if (download_item->CanResume())
        percent = download_item->PercentComplete();

      // TODO(asanka): last_reason_text should be set via
      // download_model.GetInterruptReasonText(). But we are using
      // GetStatusText() as a temporary measure until the layout is fixed to
      // accommodate the longer string. http://crbug.com/609255
      last_reason_text = download_model.GetStatusText();
      if (download::DOWNLOAD_INTERRUPT_REASON_CRASH ==
              download_item->GetLastReason() &&
          !download_item->CanResume()) {
        retry = true;
      }
      break;

    case download::DownloadItem::CANCELLED:
      state = "CANCELLED";
      retry = true;
      break;

    case download::DownloadItem::COMPLETE:
      DCHECK(!download_item->IsDangerous());
      state = "COMPLETE";
      break;

    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
  }

  DCHECK(state);

  file_value->danger_type = danger_type;
  file_value->last_reason_text = base::UTF16ToUTF8(last_reason_text);
  file_value->percent = percent;
  file_value->progress_status_text = base::UTF16ToUTF8(progress_status_text);
  file_value->retry = retry;
  file_value->state = state;

  return file_value;
}

bool DownloadsListTracker::IsIncognito(const DownloadItem& item) const {
  return GetOriginalNotifierManager() && GetMainNotifierManager() &&
      GetMainNotifierManager()->GetDownload(item.GetId()) == &item;
}

const DownloadItem* DownloadsListTracker::GetItemForTesting(size_t index)
    const {
  if (index >= sorted_items_.size())
    return nullptr;

  auto it = sorted_items_.begin();
  std::advance(it, index);
  return *it;
}

void DownloadsListTracker::SetChunkSizeForTesting(size_t chunk_size) {
  CHECK_EQ(0u, sent_to_page_);
  chunk_size_ = chunk_size;
}

bool DownloadsListTracker::ShouldShow(const DownloadItem& item) const {
  return !download_crx_util::IsTrustedExtensionDownload(
             Profile::FromBrowserContext(
                 GetMainNotifierManager()->GetBrowserContext()),
             item) &&
         !item.IsTemporary() && !item.IsTransient() &&
         !item.GetFileNameToReportUser().empty() &&
         !item.GetTargetFilePath().empty() && !item.GetURL().is_empty() &&
         DownloadItemModel(const_cast<DownloadItem*>(&item))
             .ShouldShowInShelf() &&
         DownloadQuery::MatchesQuery(search_terms_, item);
}

bool DownloadsListTracker::StartTimeComparator::operator()(
    const download::DownloadItem* a,
    const download::DownloadItem* b) const {
  return a->GetStartTime() > b->GetStartTime();
}

void DownloadsListTracker::Init() {
  Profile* profile = Profile::FromBrowserContext(
      GetMainNotifierManager()->GetBrowserContext());
  if (profile->IsOffTheRecord()) {
    Profile* original_profile = profile->GetOriginalProfile();
    original_notifier_ = std::make_unique<download::AllDownloadItemNotifier>(
        BrowserContext::GetDownloadManager(original_profile), this);
  }

  RebuildSortedItems();
}

void DownloadsListTracker::RebuildSortedItems() {
  DownloadVector all_items, visible_items;

  GetMainNotifierManager()->GetAllDownloads(&all_items);

  if (GetOriginalNotifierManager())
    GetOriginalNotifierManager()->GetAllDownloads(&all_items);

  DownloadQuery query;
  query.AddFilter(should_show_);
  query.Search(all_items.begin(), all_items.end(), &visible_items);

  SortedSet sorted_items(visible_items.begin(), visible_items.end());
  sorted_items_.swap(sorted_items);
}

void DownloadsListTracker::InsertItem(const SortedSet::iterator& insert) {
  if (!sending_updates_)
    return;

  size_t index = GetIndex(insert);
  if (index >= chunk_size_ && index >= sent_to_page_)
    return;

  std::vector<downloads::mojom::DataPtr> list;
  list.push_back(CreateDownloadData(*insert));

  page_->InsertItems(static_cast<int>(index), std::move(list));

  sent_to_page_++;
}

void DownloadsListTracker::UpdateItem(const SortedSet::iterator& update) {
  if (!sending_updates_ || GetIndex(update) >= sent_to_page_)
    return;

  page_->UpdateItem(static_cast<int>(GetIndex(update)),
                    CreateDownloadData(*update));
}

size_t DownloadsListTracker::GetIndex(const SortedSet::iterator& item) const {
  // TODO(dbeam): this could be log(N) if |item| was random access.
  return std::distance(sorted_items_.begin(), item);
}

void DownloadsListTracker::RemoveItem(const SortedSet::iterator& remove) {
  if (sending_updates_) {
    size_t index = GetIndex(remove);

    if (index < sent_to_page_) {
      page_->RemoveItem(static_cast<int>(index));
      sent_to_page_--;
    }
  }
  sorted_items_.erase(remove);
}
