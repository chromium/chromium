// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads/downloads_list_tracker.h"

#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/i18n/unicodestring.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_query.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/download_ui_safe_browsing_util.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_item_rename_handler.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "extensions/browser/extension_registry.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/filename_util.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "ui/base/l10n/time_format.h"
#include "url/url_constants.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#endif

using content::BrowserContext;
using content::DownloadManager;
using download::DownloadItem;
using TailoredWarningType = DownloadUIModel::TailoredWarningType;

using DownloadVector = DownloadManager::DownloadVector;

namespace {

// Returns an enum value to be used as the |danger_type| value in
// CreateDownloadData().
downloads::mojom::DangerType GetDangerType(
    download::DownloadDangerType danger_type) {
  switch (danger_type) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return downloads::mojom::DangerType::kDangerousFile;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return downloads::mojom::DangerType::kDangerousUrl;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return downloads::mojom::DangerType::kDangerousContent;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      return downloads::mojom::DangerType::kCookieTheft;
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return downloads::mojom::DangerType::kUncommonContent;
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return downloads::mojom::DangerType::kDangerousHost;
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return downloads::mojom::DangerType::kPotentiallyUnwanted;
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING:
      return downloads::mojom::DangerType::kAsyncScanning;
    case download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING:
      return downloads::mojom::DangerType::kAsyncLocalPasswordScanning;
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED:
      return downloads::mojom::DangerType::kBlockedPasswordProtected;
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_TOO_LARGE:
      return downloads::mojom::DangerType::kBlockedTooLarge;
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING:
      return downloads::mojom::DangerType::kSensitiveContentWarning;
    case download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK:
      return downloads::mojom::DangerType::kSensitiveContentBlock;
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED:
      return downloads::mojom::DangerType::kDeepScannedFailed;
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE:
      return downloads::mojom::DangerType::kDeepScannedSafe;
    case download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_OPENED_DANGEROUS:
      return downloads::mojom::DangerType::kDeepScannedOpenedDangerous;
    case download::DOWNLOAD_DANGER_TYPE_BLOCKED_SCAN_FAILED:
      return downloads::mojom::DangerType::kBlockedScanFailed;
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case download::DOWNLOAD_DANGER_TYPE_ALLOWLISTED_BY_POLICY:
    case download::DOWNLOAD_DANGER_TYPE_MAX:
      return downloads::mojom::DangerType::kNoApplicableDangerType;
  }
}

// Returns an enum value to be used as the |tailored_warning_type| value in
// CreateDownloadData().
downloads::mojom::TailoredWarningType GetTailoredWarningType(
    TailoredWarningType tailored_warning_type) {
  switch (tailored_warning_type) {
    case TailoredWarningType::kSuspiciousArchive:
      return downloads::mojom::TailoredWarningType::kSuspiciousArchive;
    case TailoredWarningType::kCookieTheft:
      return downloads::mojom::TailoredWarningType::kCookieTheft;
    case TailoredWarningType::kCookieTheftWithAccountInfo:
      return downloads::mojom::TailoredWarningType::kCookieTheftWithAccountInfo;
    case TailoredWarningType::kNoTailoredWarning:
      return downloads::mojom::TailoredWarningType::
          kNoApplicableTailoredWarningType;
  }
}

downloads::mojom::SafeBrowsingState GetSafeBrowsingState(Profile* profile) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::SafeBrowsingState state =
      safe_browsing::GetSafeBrowsingState(*profile->GetPrefs());
  switch (state) {
    case safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING:
      return downloads::mojom::SafeBrowsingState::kNoSafeBrowsing;
    case safe_browsing::SafeBrowsingState::STANDARD_PROTECTION:
      return downloads::mojom::SafeBrowsingState::kStandardProtection;
    case safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION:
      return downloads::mojom::SafeBrowsingState::kStandardProtection;
  }
#else
  return downloads::mojom::SafeBrowsingState::kNoSafeBrowsing;
#endif
}

// TODO(dbeam): if useful elsewhere, move to base/i18n/time_formatting.h?
std::string TimeFormatLongDate(const base::Time& time) {
  std::unique_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kLong));
  icu::UnicodeString date_string;
  formatter->format(time.InMillisecondsFSinceUnixEpoch(), date_string);
  return base::UTF16ToUTF8(base::i18n::UnicodeStringToString16(date_string));
}

std::u16string GetFormattedDisplayUrl(const GURL& url) {
  std::u16string result = url_formatter::FormatUrlForSecurityDisplay(url);
  // Truncate long URL to avoid surpassing mojo data limit (c.f.
  // crbug.com/1070451). If it's really this long, the user won't be able to see
  // the whole thing anyway. We truncate the beginning so that the end of it is
  // shown, which contains the eTLD+1.
  // Note:
  // - This may truncate the scheme part of the URL.
  // - Use a much smaller limit than url::kMaxURLChars (2M) since this is for
  //   display only, and long URLs will affect page load speed and may cause
  //   JavaScript errors (https://crbug.com/1522764).
  const size_t kMaxDisplayURLChars = 16 * 1024;
  if (result.size() > kMaxDisplayURLChars) {
    result = result.substr(result.size() - kMaxDisplayURLChars);
  }
  return result;
}

void FillUrlFields(const GURL& url,
                   std::optional<GURL>& data_url,
                   std::u16string& display_url_out) {
  // If URL is too long, don't make it clickable.
  if (url.is_valid() && url.spec().length() <= url::kMaxURLChars) {
    data_url = std::make_optional<GURL>(url);
  }

  display_url_out = GetFormattedDisplayUrl(url);
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
  std::vector<std::u16string> new_terms;
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

int DownloadsListTracker::NumDangerousItemsSent() const {
  auto sent_items_end_it = sorted_items_.begin();
  std::advance(sent_items_end_it, sent_to_page_);

  return base::ranges::count_if(
      sorted_items_.begin(), sent_items_end_it,
      [](download::DownloadItem* item) { return item->IsDangerous(); });
}

download::DownloadItem* DownloadsListTracker::GetFirstActiveWarningItem() {
  auto sent_items_end_it = sorted_items_.begin();
  std::advance(sent_items_end_it, sent_to_page_);

  auto iter = base::ranges::find_if(
      sorted_items_.begin(), sent_items_end_it,
      [](download::DownloadItem* item) {
        return item->GetState() != download::DownloadItem::CANCELLED &&
               item->IsDangerous();
      });
  if (iter != sent_items_end_it) {
    return *iter;
  }
  return nullptr;
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
    base::RepeatingCallback<bool(const DownloadItem&)> should_show)
    : main_notifier_(download_manager, this),
      page_(std::move(page)),
      should_show_(std::move(should_show)) {
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
      ui::TimeFormat::RelativeDate(download_item->GetStartTime(), nullptr));
  file_value->date_string = TimeFormatLongDate(download_item->GetStartTime());

  file_value->id = base::NumberToString(download_item->GetId());

  base::FilePath download_path(download_item->GetTargetFilePath());
  file_value->file_path = download_path.AsUTF8Unsafe();
  GURL file_url = net::FilePathToFileURL(download_path);
  if (file_url.is_valid()) {
    file_value->file_url = file_url.spec();
  }

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
  std::u16string file_name =
      download_item->GetFileNameToReportUser().LossyDisplayName();
  file_name = base::i18n::GetDisplayStringInLTRDirectionality(file_name);

  file_value->file_name = base::UTF16ToUTF8(file_name);
  FillUrlFields(download_item->GetURL(), file_value->url,
                file_value->display_url);
  if (download_item->HasUserGesture()) {
    FillUrlFields(download_item->GetReferrerUrl(), file_value->referrer_url,
                  file_value->display_referrer_url);
  }
  file_value->total = download_item->GetTotalBytes();
  file_value->file_externally_removed =
      download_item->GetFileExternallyRemoved();
  file_value->resume = download_item->CanResume();
  file_value->otr = IsIncognito(*download_item);

  std::u16string last_reason_text;
  // -2 is invalid, -1 means indeterminate, and 0-100 are in-progress.
  int percent = -2;
  std::u16string progress_status_text;
  bool retry = false;
  // This will always be populated, but we set a null value to start with.
  std::optional<downloads::mojom::State> state = std::nullopt;

  switch (download_item->GetState()) {
    case download::DownloadItem::IN_PROGRESS: {
      if (download_item->GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING) {
        state = downloads::mojom::State::kPromptForScanning;
      } else if (download_item->GetDangerType() ==
                 download::
                     DOWNLOAD_DANGER_TYPE_PROMPT_FOR_LOCAL_PASSWORD_SCANNING) {
        state = downloads::mojom::State::kPromptForLocalPasswordScanning;
      } else if (download_item->GetDangerType() ==
                 download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING) {
        state = downloads::mojom::State::kAsyncScanning;
      } else if (download_item->IsDangerous()) {
        state = downloads::mojom::State::kDangerous;
      } else if (download_item->IsInsecure()) {
        state = downloads::mojom::State::kInsecure;
      } else if (download_item->IsPaused()) {
        state = downloads::mojom::State::kPaused;
      } else {
        state = downloads::mojom::State::kInProgress;
      }
      progress_status_text = download_model.GetTabProgressStatusText();
      percent = GetPercentComplete(download_item);
      break;
    }

    case download::DownloadItem::INTERRUPTED:
      state = downloads::mojom::State::kInterrupted;
      progress_status_text = download_model.GetTabProgressStatusText();

      if (download_item->CanResume())
        percent = GetPercentComplete(download_item);

      // TODO(crbug.com/40467967): GetHistoryPageStatusText() is using
      // GetStatusText() as a temporary measure until the layout is fixed to
      // accommodate the longer string. Should update it to simply use
      // GetInterruptDescription().
      last_reason_text = download_model.GetHistoryPageStatusText();
      if (download::DOWNLOAD_INTERRUPT_REASON_CRASH ==
              download_item->GetLastReason() &&
          !download_item->CanResume()) {
        retry = true;
      }
      break;

    case download::DownloadItem::CANCELLED:
      state = downloads::mojom::State::kCancelled;
      retry = true;
      break;

    case download::DownloadItem::COMPLETE:
      DCHECK(!download_item->IsDangerous());
      state = downloads::mojom::State::kComplete;
      break;

    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED_IN_MIGRATION();
  }

  CHECK(state);

  downloads::mojom::DangerType danger_type =
      GetDangerType(download_item->GetDangerType());
  downloads::mojom::TailoredWarningType tailored_warning_type =
      GetTailoredWarningType(download_model.GetTailoredWarningType());
  file_value->danger_type = danger_type;
  file_value->tailored_warning_type = tailored_warning_type;
  file_value->is_dangerous = download_item->IsDangerous();
  file_value->is_insecure = download_item->IsInsecure();
  file_value->is_reviewable =
      enterprise_connectors::ShouldPromptReviewForDownload(
          Profile::FromBrowserContext(
              content::DownloadItemUtils::GetBrowserContext(download_item)),
          download_item);

  file_value->last_reason_text = base::UTF16ToUTF8(last_reason_text);
  file_value->percent = percent;
  file_value->progress_status_text = base::UTF16ToUTF8(progress_status_text);
  file_value->show_in_folder_text =
      base::UTF16ToUTF8(download_model.GetShowInFolderText());
  file_value->retry = retry;
  file_value->state = *state;

  // Note that the safe_browsing_state is the state of the download's profile
  // *now* whereas the presence of a verdict was determined when the download
  // happened, so they are not necessarily related.
  file_value->safe_browsing_state =
      GetSafeBrowsingState(download_model.profile());
  file_value->has_safe_browsing_verdict =
      WasSafeBrowsingVerdictObtained(download_item);

  MaybeRecordDangerousDownloadWarningShown(download_model);

  if (download_item->IsDangerous()) {
    // It's likely that SHOWN has already been logged from the download bubble,
    // but in a small number of cases the warning may not have been shown in
    // the bubble but is shown for the first time on the downloads page instead.
    // That case is captured here. The majority of the time, the logic in
    // DownloadItemWarningData that prevents double-logging will make this a
    // no-op (aside from logging a histogram).
    DownloadItemWarningData::AddWarningActionEvent(
        download_item, DownloadItemWarningData::WarningSurface::DOWNLOADS_PAGE,
        DownloadItemWarningData::WarningAction::SHOWN);
  }

  if (tailored_warning_type ==
      downloads::mojom::TailoredWarningType::kCookieTheftWithAccountInfo) {
    if (auto* identity_manager =
            IdentityManagerFactory::GetForProfile(download_model.profile());
        identity_manager) {
      std::string email =
          identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
              .email;
      if (!email.empty()) {
        file_value->account_email = std::move(email);
      }
    }
  }

  return file_value;
}

bool DownloadsListTracker::IsIncognito(const DownloadItem& item) const {
  return GetOriginalNotifierManager() && GetMainNotifierManager() &&
         GetMainNotifierManager()->GetDownload(item.GetId()) == &item;
}

const DownloadItem* DownloadsListTracker::GetItemForTesting(
    size_t index) const {
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
        original_profile->GetDownloadManager(), this);
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

int DownloadsListTracker::GetPercentComplete(
    download::DownloadItem* download_item) const {
  auto* renamer = download_item->GetRenameHandler();
  if (renamer && renamer->ShowRenameProgress()) {
    return static_cast<int>(((download_item->GetReceivedBytes() +
                              download_item->GetUploadedBytes()) *
                             0.5 * 100.0) /
                            download_item->GetTotalBytes());
  } else {
    return download_item->PercentComplete();
  }
}
