// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_downloads/downloads_list_tracker.h"

#include <iterator>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/i18n/unicodestring.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_query.h"
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_system.h"
#include "net/base/filename_util.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "ui/base/l10n/time_format.h"

using content::BrowserContext;
using download::DownloadItem;
using content::DownloadManager;

using DownloadVector = DownloadManager::DownloadVector;

namespace {

// Returns a string constant to be used as the |danger_type| value in
// CreateDownloadItemValue().  Only return strings for DANGEROUS_FILE,
// DANGEROUS_URL, DANGEROUS_CONTENT, and UNCOMMON_CONTENT because the
// |danger_type| value is only defined if the value of |state| is |DANGEROUS|.
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
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_WHITELISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      break;
  }
  // Don't return a danger type string if it is NOT_DANGEROUS,
  // MAYBE_DANGEROUS_CONTENT, or USER_VALIDATED.
  NOTREACHED();
  return "";
}

// TODO(dbeam): if useful elsewhere, move to base/i18n/time_formatting.h?
base::string16 TimeFormatLongDate(const base::Time& time) {
  std::unique_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kLong));
  icu::UnicodeString date_string;
  formatter->format(static_cast<UDate>(time.ToDoubleT() * 1000), date_string);
  return base::i18n::UnicodeStringToString16(date_string);
}

}  // namespace

DownloadsListTracker::DownloadsListTracker(
    DownloadManager* download_manager,
    content::WebUI* web_ui)
  : main_notifier_(download_manager, this),
    web_ui_(web_ui),
    should_show_(base::Bind(&DownloadsListTracker::ShouldShow,
                            base::Unretained(this))) {
  Init();
}

DownloadsListTracker::~DownloadsListTracker() {}

void DownloadsListTracker::Reset() {
  if (sending_updates_)
    web_ui_->CallJavascriptFunctionUnsafe("downloads.Manager.clearAll");
  sent_to_page_ = 0u;
}

bool DownloadsListTracker::SetSearchTerms(const base::ListValue& search_terms) {
  std::vector<base::string16> new_terms;
  new_terms.resize(search_terms.GetSize());

  for (size_t i = 0; i < search_terms.GetSize(); ++i)
    search_terms.GetString(i, &new_terms[i]);

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

  base::ListValue list;
  while (it != sorted_items_.end() && list.GetSize() < chunk_size_) {
    list.Append(CreateDownloadItemValue(*it));
    ++it;
  }

  web_ui_->CallJavascriptFunctionUnsafe(
      "downloads.Manager.insertItems",
      base::Value(static_cast<int>(sent_to_page_)), list);

  sent_to_page_ += list.GetSize();
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
    content::WebUI* web_ui,
    base::Callback<bool(const DownloadItem&)> should_show)
  : main_notifier_(download_manager, this),
    web_ui_(web_ui),
    should_show_(should_show) {
  Init();
}

std::unique_ptr<base::DictionaryValue>
DownloadsListTracker::CreateDownloadItemValue(
    download::DownloadItem* download_item) const {
  // TODO(asanka): Move towards using download_model here for getting status and
  // progress. The difference currently only matters to Drive downloads and
  // those don't show up on the downloads page, but should.
  DownloadItemModel download_model(download_item);

  // The items which are to be written into file_value are also described in
  // chrome/browser/resources/downloads/downloads.js in @typedef for
  // BackendDownloadObject. Please update it whenever you add or remove
  // any keys in file_value.
  std::unique_ptr<base::DictionaryValue> file_value(new base::DictionaryValue);

  file_value->SetInteger(
      "started", static_cast<int>(download_item->GetStartTime().ToTimeT()));
  file_value->SetString(
      "since_string", ui::TimeFormat::RelativeDate(
          download_item->GetStartTime(), NULL));
  file_value->SetString(
      "date_string", TimeFormatLongDate(download_item->GetStartTime()));

  file_value->SetString("id", base::NumberToString(download_item->GetId()));

  base::FilePath download_path(download_item->GetTargetFilePath());
  file_value->SetKey("file_path", base::CreateFilePathValue(download_path));
  file_value->SetString("file_url",
                        net::FilePathToFileURL(download_path).spec());

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
    bool include_disabled = true;
    auto* profile = Profile::FromBrowserContext(
        content::DownloadItemUtils::GetBrowserContext(download_item));
    auto* service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    const extensions::Extension* extension =
        service->GetExtensionById(by_ext->id(), include_disabled);
    if (extension)
      by_ext_name = extension->name();
  }
  file_value->SetString("by_ext_id", by_ext_id);
  file_value->SetString("by_ext_name", by_ext_name);

  // Keep file names as LTR. TODO(dbeam): why?
  base::string16 file_name =
      download_item->GetFileNameToReportUser().LossyDisplayName();
  file_name = base::i18n::GetDisplayStringInLTRDirectionality(file_name);
  file_value->SetString("file_name", file_name);
  file_value->SetString("url", download_item->GetURL().spec());
  file_value->SetInteger("total", static_cast<int>(
      download_item->GetTotalBytes()));
  file_value->SetBoolean("file_externally_removed",
                         download_item->GetFileExternallyRemoved());
  file_value->SetBoolean("resume", download_item->CanResume());
  file_value->SetBoolean("otr", IsIncognito(*download_item));

  const char* danger_type = "";
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
        danger_type = GetDangerTypeString(download_item->GetDangerType());
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

  file_value->SetString("danger_type", danger_type);
  file_value->SetString("last_reason_text", last_reason_text);
  file_value->SetInteger("percent", percent);
  file_value->SetString("progress_status_text", progress_status_text);
  file_value->SetBoolean("retry", retry);
  file_value->SetString("state", state);

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
  return !download_crx_util::IsExtensionDownload(item) && !item.IsTemporary() &&
         !item.IsTransient() && !item.GetFileNameToReportUser().empty() &&
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
    original_notifier_.reset(new download::AllDownloadItemNotifier(
        BrowserContext::GetDownloadManager(original_profile), this));
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

  base::ListValue list;
  list.Append(CreateDownloadItemValue(*insert));

  web_ui_->CallJavascriptFunctionUnsafe("downloads.Manager.insertItems",
                                        base::Value(static_cast<int>(index)),
                                        list);

  sent_to_page_++;
}

void DownloadsListTracker::UpdateItem(const SortedSet::iterator& update) {
  if (!sending_updates_ || GetIndex(update) >= sent_to_page_)
    return;

  web_ui_->CallJavascriptFunctionUnsafe(
      "downloads.Manager.updateItem",
      base::Value(static_cast<int>(GetIndex(update))),
      *CreateDownloadItemValue(*update));
}

size_t DownloadsListTracker::GetIndex(const SortedSet::iterator& item) const {
  // TODO(dbeam): this could be log(N) if |item| was random access.
  return std::distance(sorted_items_.begin(), item);
}

void DownloadsListTracker::RemoveItem(const SortedSet::iterator& remove) {
  if (sending_updates_) {
    size_t index = GetIndex(remove);
    if (index < sent_to_page_) {
      web_ui_->CallJavascriptFunctionUnsafe(
          "downloads.Manager.removeItem", base::Value(static_cast<int>(index)));
      sent_to_page_--;
    }
  }
  sorted_items_.erase(remove);
}
