// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/shortcuts_backend.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "components/history/core/browser/history_backend.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "components/omnibox/browser/shortcuts_database.h"
#include "components/omnibox/browser/tailored_word_break_iterator.h"
#include "components/omnibox/common/omnibox_features.h"

namespace {

// The amount of time, in minutes, to wait after initialization before
// attempting to expire old shortcuts. Used to avoid contention with other work
// performed on profile loading and to outwait the 30 second delay before
// `ExpireHistoryBackend` starts history deletions, in case the initialization
// of that and `ShortcutsBackend` happen around the same time.
const int kInitialExpirationDelayMinutes = 2;

// Takes Match classification vector and removes all matched positions,
// compacting repetitions if necessary.
std::string StripMatchMarkers(const ACMatchClassifications& matches) {
  ACMatchClassifications unmatched;
  for (const auto& match : matches) {
    AutocompleteMatch::AddLastClassificationIfNecessary(
        &unmatched, match.offset, match.style & ~ACMatchClassification::MATCH);
  }
  return AutocompleteMatch::ClassificationsToString(unmatched);
}

// Normally shortcuts have the same match type as the original match they were
// created from, but for certain match types, we should modify the shortcut's
// type slightly to reflect that the origin of the shortcut is historical.
AutocompleteMatch::Type GetTypeForShortcut(AutocompleteMatch::Type type) {
  switch (type) {
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
    case AutocompleteMatchType::NAVSUGGEST:
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED:
      return AutocompleteMatchType::HISTORY_URL;

    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
      return type;

    default:
      return AutocompleteMatch::IsSearchType(type)
                 ? AutocompleteMatchType::SEARCH_HISTORY
                 : type;
  }
}

// Expand the last word in `text` to a full word in `match_text`. E.g., if
// `text` is 'Cha Aznav' and the `match_text` is 'Charles Aznavour', will return
// 'Cha Aznavour'. Inlining 'Cha Aznav' would look incomplete.
// - `trimmed_text` and `match_text` should have original capitalization as
//   `ExpandToFullWord()` tries to preserve it.
// - `trimmed_text` should be trimmed for efficiency since the callers already
//   have it available.
// - `is_existing_shortcut` is true when updating existing shortcuts and false
//    when creating new shortcuts. See comment below.
std::u16string ExpandToFullWord(std::u16string trimmed_text,
                                std::u16string match_text,
                                bool is_existing_shortcut) {
  DCHECK(!trimmed_text.empty());

  // `trimmed_text` should be trimmed to:
  // 1) Avoid expanding, e.g., the `text` 'Cha Aznav ' to 'Cha Aznav ur'.
  // 2) Avoid truncating the shortcut e.g., 'Cha Aznavour' to 'Cha ' for the
  //    `text` 'C' when `AddOrUpdateShortcut()` appends 3 chars to `text`.
  // 3) Allow expanding, e.g., the `text` 'Cha ' to 'Charles'.
  // 4) Even when not expanding, autocompleting trailing whitespace looks weird.
  CHECK_EQ(trimmed_text, base::TrimWhitespace(
                             trimmed_text, base::TrimPositions::TRIM_TRAILING));

  // Preserving original `match_text` case is ideal; it allows autocompleting
  // 'char[les Aznavour] instead of 'char[les aznavour]'. But some characters
  // have different lengths in lower v upper case; e.g., 'İ' v 'i̇' (i + ̇).
  // That's problematic when trying to find the exact sub-words to autocomplete:
  //  - Best case, it autocompletes incorrectly; e.g. 'char[es]', 'char[rles]',
  //    or 'char[arles]'.
  //  - Worst case, it crashes when trimming string out of bounds.
  // So fallback to lower casing the match text when lengths don't match.
  const auto lower_match_text = base::i18n::ToLower(match_text);
  if (match_text.length() != lower_match_text.length())
    match_text = lower_match_text;

  // Adopt extra chars from the existing shortcut to avoid unstable
  // shortcuts. E.g. if the user types 'Charl', keep the shortcut text
  // 'Charles Aznavour' instead of truncating it to 'Charles'.
  if (is_existing_shortcut) {
    // 3 chars is sufficient to keep shortcuts meaningful while reducing them
    // to what the user tends to type.
    constexpr int kCharCount = 3;
    trimmed_text = base::StrCat(
        {trimmed_text,
         match_text.substr(base::i18n::ToLower(trimmed_text).length(),
                           kCharCount)});
    trimmed_text =
        base::TrimWhitespace(trimmed_text, base::TrimPositions::TRIM_TRAILING);
  }

  // Use the lower cased text for string insensitive comparisons. Use the
  // original case to construct the returned expanded text. E.g., 'cHa' should
  // expand to 'cHarles', not 'Charles'.
  const auto trimmed_lower_text = base::i18n::ToLower(trimmed_text);

  // There may be multiple matching match words `text` can be expanded to. E.g.
  // with `text` 'x' and `match_text` 'x1 x2', 'x' matches both 'x1' and 'x2'.
  // 1) If `text` is a prefix of `match_text` (after trimming `text` whitespace
  //    and ignoring case), then pick the next match word.
  //    - E.g., 'x1 x' will expand to 'x1 x2', not 'x1 x1'.
  // 2) Otherwise, pick the 1st match word at least 3 chars long.
  //    - Prefer the 1st match word because the alternatives (e.g., the longest,
  //      shortest, or match closest to the previous word) all have undesirable
  //      edge cases. E.g., if using the longest match, the `text` 'singer C',
  //      with match description 'Singer Charles Aznavour Performs Les
  //      Comediens', would expand to 'singer Comédiens'.
  //    - Prefer words at least 3 chars long to avoid expanding to 'a', 'at',
  //      'to', etc when a more likely candidate exists.
  //    - E.g., 'x1 x x' will expand to 'x1 x x1', not 'x1 x x2'.
  // 3) Otherwise, resort to the 1st match word of any length.
  //    - E.g, With `match_text` 'a ab xy xyz mn'
  //      'x' will expand to 'xyz', not 'xy'.
  //      'a' will expand to 'a', not 'ab'.
  //      'm' will expand to 'mn'.

  // This handles case (1) from the above comment.
  if (base::StartsWith(lower_match_text, trimmed_lower_text,
                       base::CompareCase::SENSITIVE)) {
    // Cut off the common prefix.
    const auto cut_match_text = match_text.substr(trimmed_lower_text.length());
    // Find the 1st word of the cut `match_text`.
    TailoredWordBreakIterator iter(cut_match_text);
    // Append that word to the text.
    if (iter.Init() && iter.Advance() && iter.IsWord())
      return base::StrCat({trimmed_text, iter.GetString()});
  }

  // Find the last word in `text` to expand.
  WordStarts text_word_starts;
  const auto text_words =
      String16VectorFromString16(trimmed_lower_text, &text_word_starts);
  // `String16VectorFromString16()` only considers the 1st 200
  // (`kMaxSignificantChars`) chars for word starts while it considers the full
  // text for words.
  DCHECK_LE(text_word_starts.size(), text_words.size());
  // Even though `text` won't be empty, it may contain no words if it consists
  // of only symbols and whitespace. Additionally, even if it does contain
  // words, if it ends with symbols, the last word shouldn't be expanded to
  // avoid expanding, e.g., the text 'Cha*' to 'Cha*rles'.
  if (text_word_starts.empty() ||
      text_word_starts.back() + text_words.back().length() !=
          trimmed_lower_text.length()) {
    return trimmed_text;
  }
  const auto& text_last_word = text_words.back();

  // This handles cases (2) and (3) from the above comment.
  const auto match_words = String16VectorFromString16(match_text, nullptr);
  std::u16string best_word;
  // Iterate up to 100 `match_words` for performance.
  for (size_t i = 0;
       i < match_words.size() && i < 100 && best_word.length() < 3u; ++i) {
    if (match_words[i].length() < 3u && !best_word.empty())
      continue;
    if (!base::StartsWith(base::i18n::ToLower(match_words[i]), text_last_word,
                          base::CompareCase::SENSITIVE))
      continue;
    best_word = match_words[i];
  }

  // Add on the missing letters of `text_last_word`, rather than replace it with
  // `best_word` to preserve capitalization.
  return best_word.empty()
             ? trimmed_text
             : base::StrCat(
                   {trimmed_text, best_word.substr(text_last_word.length())});
}

}  // namespace

// ShortcutsBackend -----------------------------------------------------------

// static
const std::u16string& ShortcutsBackend::GetDescription(
    const AutocompleteMatch& match) {
  return match.swap_contents_and_description ||
                 match.description_for_shortcuts.empty()
             ? match.description
             : match.description_for_shortcuts;
}

// static
const std::u16string& ShortcutsBackend::GetSwappedDescription(
    const AutocompleteMatch& match) {
  return match.swap_contents_and_description ? GetContents(match)
                                             : GetDescription(match);
}

// static
const ACMatchClassifications& ShortcutsBackend::GetDescriptionClass(
    const AutocompleteMatch& match) {
  return match.swap_contents_and_description ||
                 match.description_class_for_shortcuts.empty()
             ? match.description_class
             : match.description_class_for_shortcuts;
}

// static
const std::u16string& ShortcutsBackend::GetContents(
    const AutocompleteMatch& match) {
  return !match.swap_contents_and_description ||
                 match.description_for_shortcuts.empty()
             ? match.contents
             : match.description_for_shortcuts;
}

// static
const std::u16string& ShortcutsBackend::GetSwappedContents(
    const AutocompleteMatch& match) {
  return match.swap_contents_and_description ? match.description
                                             : match.contents;
}

// static
const ACMatchClassifications& ShortcutsBackend::GetContentsClass(
    const AutocompleteMatch& match) {
  return !match.swap_contents_and_description ||
                 match.description_class_for_shortcuts.empty()
             ? match.contents_class
             : match.description_class_for_shortcuts;
}

ShortcutsBackend::ShortcutsBackend(
    TemplateURLService* template_url_service,
    std::unique_ptr<SearchTermsData> search_terms_data,
    history::HistoryService* history_service,
    base::FilePath database_path,
    bool suppress_db)
    : template_url_service_(template_url_service),
      search_terms_data_(std::move(search_terms_data)),
      current_state_(NOT_INITIALIZED),
      main_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      db_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      no_db_access_(suppress_db) {
  if (!suppress_db)
    db_ = new ShortcutsDatabase(database_path);
  if (history_service)
    history_service_observation_.Observe(history_service);
}

bool ShortcutsBackend::Init() {
  if (current_state_ != NOT_INITIALIZED)
    return false;

  if (no_db_access_) {
    current_state_ = INITIALIZED;
    return true;
  }

  current_state_ = INITIALIZING;
  return db_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ShortcutsBackend::InitInternal, this));
}

bool ShortcutsBackend::DeleteShortcutsWithURL(const GURL& shortcut_url) {
  return initialized() && DeleteShortcutsWithURL(shortcut_url, true);
}

bool ShortcutsBackend::DeleteShortcutsBeginningWithURL(
    const GURL& shortcut_url) {
  return initialized() && DeleteShortcutsWithURL(shortcut_url, false);
}

void ShortcutsBackend::AddObserver(ShortcutsBackendObserver* obs) {
  observer_list_.AddObserver(obs);
}

void ShortcutsBackend::RemoveObserver(ShortcutsBackendObserver* obs) {
  observer_list_.RemoveObserver(obs);
}

void ShortcutsBackend::AddOrUpdateShortcut(const std::u16string& text,
                                           const AutocompleteMatch& match) {
#if DCHECK_IS_ON()
  match.Validate();
#endif  // DCHECK_IS_ON()

  // Trim `text` since `ExpandToFullWord()` trims the shortcut text; otherwise,
  // inputs with trailing whitespace wouldn't match a shortcut even if the user
  // previously used the input with a trailing whitespace.
  const std::u16string text_trimmed = std::u16string(
      base::TrimWhitespace(text, base::TrimPositions::TRIM_TRAILING));

  // `text` may be empty for pedal and zero suggest navigations. `text_trimmed`
  // can additionally be empty for whitespace-only inputs. It's unlikely users
  // will have a predictable navigation with such inputs, so early exit.
  // Besides, `ShortcutsProvider::Start()` also early exits on empty inputs, so
  // there's no reason to add empty-text shortcuts if they won't be used.
  if (text_trimmed.empty())
    return;

  // On mobile on focus, zero suggest navigations have a non-empty `text` (it
  // contains the current page URL). Ignore these navigations as shortcut
  // suggestions are not provided in zero suggest.
  if (match.provider &&
      match.provider->type() == AutocompleteProvider::TYPE_ZERO_SUGGEST) {
    return;
  }

  const std::u16string text_trimmed_lowercase(
      base::i18n::ToLower(text_trimmed));
  const base::Time now(base::Time::Now());

  // Look for an existing shortcut to `match` prefixed by `text`. If there is
  // one, it'll be updated. This avoids creating duplicating equivalent
  // shortcuts (e.g. 'g', 'go', & 'goo') with distributed `number_of_hits`s and
  // outdated `last_access_time`s. There could be multiple relevant shortcuts;
  // e.g., the `text` 'wi' could match both shortcuts 'wiki' and 'wild' to
  // 'wiki.org/wild_west'. We only update the 1st shortcut; this is slightly
  // arbitrary but seems to be fine. Deduping these shortcuts would stop the
  // input 'wil' from finding the 2nd shortcut.
  for (ShortcutMap::const_iterator it(
           shortcuts_map_.lower_bound(text_trimmed_lowercase));
       it != shortcuts_map_.end() &&
       base::StartsWith(it->first, text_trimmed_lowercase,
                        base::CompareCase::SENSITIVE);
       ++it) {
    if (match.destination_url == it->second.match_core.destination_url) {
      // When a user navigates to a shortcut after typing a prefix of the
      // shortcut, the shortcut text is replaced with the shorter user input.
      const auto expanded_text =
          ExpandToFullWord(text_trimmed, it->second.text, true);
      UpdateShortcut(ShortcutsDatabase::Shortcut(
          it->second.id, expanded_text,
          MatchToMatchCore(match, template_url_service_,
                           search_terms_data_.get()),
          now, it->second.number_of_hits + 1));
      return;
    }
  }

  // If no shortcuts to `match` prefixed by `text` were found, create one.

  // Try to expand the input to a full word so that inputs like 'Aram Kha' later
  // autocomplete 'Aram Khachaturian' instead of the incomplete input. Prefer
  // `contents` as the `description` & URL are usually less meaningful (e.g.
  // 'docs.google.com/d/3SyB0Y83dG_WuxX' or 'Google Search'). Except when
  // `swap_contents_and_description` is true, which means the description
  // contains the title or meaningful text. Also consider the URL host, which
  // is usually also recognizable and helpful when there are whitespace or other
  // discrepancies between the title and host (e.g. 'Stack Overflow' and
  // 'stackoverflow.com').
  const auto expanded_text =
      ExpandToFullWord(text_trimmed,
                       GetSwappedContents(match) + u" " +
                           base::UTF8ToUTF16(match.destination_url.host()),
                       false);
  AddShortcut(ShortcutsDatabase::Shortcut(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), expanded_text,
      MatchToMatchCore(match, template_url_service_, search_terms_data_.get()),
      now, 1));
}

ShortcutsBackend::~ShortcutsBackend() {
  db_runner_->ReleaseSoon(FROM_HERE, std::move(db_));
}

// static
ShortcutsDatabase::Shortcut::MatchCore ShortcutsBackend::MatchToMatchCore(
    const AutocompleteMatch& match,
    TemplateURLService* template_url_service,
    SearchTermsData* search_terms_data) {
  const AutocompleteMatch::Type match_type = GetTypeForShortcut(match.type);

  const AutocompleteMatch* normalized_match = &match;
  AutocompleteMatch temp;

  if (AutocompleteMatch::IsSpecializedSearchType(match.type)) {
    DCHECK(match.search_terms_args);
    temp = BaseSearchProvider::CreateShortcutSearchSuggestion(
        match.search_terms_args->search_terms, match_type,
        ui::PageTransitionCoreTypeIs(match.transition,
                                     ui::PAGE_TRANSITION_KEYWORD),
        match.GetTemplateURL(template_url_service, false), *search_terms_data);
    normalized_match = &temp;
  }

  return ShortcutsDatabase::Shortcut::MatchCore(
      normalized_match->fill_into_edit, normalized_match->destination_url,
      normalized_match->document_type, GetContents(*normalized_match),
      StripMatchMarkers(GetContentsClass(*normalized_match)),
      GetDescription(*normalized_match),
      StripMatchMarkers(GetDescriptionClass(*normalized_match)),
      normalized_match->transition, match_type, normalized_match->keyword);
}

void ShortcutsBackend::ShutdownOnUIThread() {
  history_service_observation_.Reset();
  template_url_service_ = nullptr;
}

void ShortcutsBackend::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (!initialized())
    return;

  if (deletion_info.IsAllHistory()) {
    DeleteAllShortcuts();
    return;
  }

  ShortcutsDatabase::ShortcutIDs shortcut_ids;
  for (const auto& guid_pair : guid_map_) {
    if (base::ranges::any_of(
            deletion_info.deleted_rows(),
            history::URLRow::URLRowHasURL(
                guid_pair.second->second.match_core.destination_url))) {
      shortcut_ids.push_back(guid_pair.first);
    }
  }

  UMA_HISTOGRAM_COUNTS_100(
      "ShortcutsProvider.OldEntryDeletions.OnHistoryDeletions",
      shortcut_ids.size());

  DeleteShortcutsWithIDs(shortcut_ids);
}

void ShortcutsBackend::InitInternal() {
  DCHECK(current_state_ == INITIALIZING);
  db_->Init();

  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  db_->LoadShortcuts(&shortcuts);

  temp_shortcuts_map_ = std::make_unique<ShortcutMap>();
  temp_guid_map_ = std::make_unique<GuidMap>();
  for (ShortcutsDatabase::GuidToShortcutMap::const_iterator it(
           shortcuts.begin());
       it != shortcuts.end(); ++it) {
    (*temp_guid_map_)[it->first] = temp_shortcuts_map_->insert(
        std::make_pair(base::i18n::ToLower(it->second.text), it->second));
  }

  main_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ShortcutsBackend::InitCompleted, this));
}

void ShortcutsBackend::InitCompleted() {
  temp_guid_map_->swap(guid_map_);
  temp_shortcuts_map_->swap(shortcuts_map_);
  temp_shortcuts_map_.reset(nullptr);
  temp_guid_map_.reset(nullptr);

  current_state_ = INITIALIZED;
  for (ShortcutsBackendObserver& observer : observer_list_)
    observer.OnShortcutsLoaded();

  ComputeDatabaseMetrics();

  if (base::FeatureList::IsEnabled(omnibox::kOmniboxDeleteOldShortcuts)) {
    main_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&ShortcutsBackend::DeleteOldShortcuts),
            weak_factory_.GetWeakPtr()),
        base::Minutes(kInitialExpirationDelayMinutes));
  }
}

void ShortcutsBackend::ComputeDatabaseMetrics() {
  int num_shortcuts = shortcuts_map_.size();
  UMA_HISTOGRAM_COUNTS_10000("ShortcutsProvider.DatabaseSize", num_shortcuts);

  int num_old_shortcuts = 0;
  const base::Time now(base::Time::Now());
  for (const auto& shortcut_pair : shortcuts_map_) {
    if (now - shortcut_pair.second.last_access_time >
        base::Days(history::HistoryBackend::kExpireDaysThreshold)) {
      num_old_shortcuts++;
    }
  }
  UMA_HISTOGRAM_COUNTS_10000("ShortcutsProvider.DatabaseSize.OldEntries",
                             num_old_shortcuts);

  int tenth_percent_old_shortcuts = 0;
  if (num_shortcuts > 0) {
    tenth_percent_old_shortcuts =
        static_cast<int>((num_old_shortcuts * 1000.0 / num_shortcuts));
  }
  UMA_HISTOGRAM_EXACT_LINEAR(
      "ShortcutsProvider.DatabaseSize.OldEntriesPercentage",
      tenth_percent_old_shortcuts, 1001);
}

bool ShortcutsBackend::AddShortcut(
    const ShortcutsDatabase::Shortcut& shortcut) {
  if (!initialized())
    return false;
  DCHECK(guid_map_.find(shortcut.id) == guid_map_.end());
  guid_map_[shortcut.id] = shortcuts_map_.insert(
      std::make_pair(base::i18n::ToLower(shortcut.text), shortcut));
  for (ShortcutsBackendObserver& observer : observer_list_)
    observer.OnShortcutsChanged();
  return no_db_access_ ||
         db_runner_->PostTask(
             FROM_HERE,
             base::BindOnce(base::IgnoreResult(&ShortcutsDatabase::AddShortcut),
                            db_.get(), shortcut));
}

bool ShortcutsBackend::UpdateShortcut(
    const ShortcutsDatabase::Shortcut& shortcut) {
  if (!initialized())
    return false;
  auto it(guid_map_.find(shortcut.id));
  if (it != guid_map_.end())
    shortcuts_map_.erase(it->second);
  guid_map_[shortcut.id] = shortcuts_map_.insert(
      std::make_pair(base::i18n::ToLower(shortcut.text), shortcut));
  for (ShortcutsBackendObserver& observer : observer_list_)
    observer.OnShortcutsChanged();
  return no_db_access_ ||
         db_runner_->PostTask(
             FROM_HERE, base::BindOnce(base::IgnoreResult(
                                           &ShortcutsDatabase::UpdateShortcut),
                                       db_.get(), shortcut));
}

bool ShortcutsBackend::DeleteShortcutsWithIDs(
    const ShortcutsDatabase::ShortcutIDs& shortcut_ids) {
  if (!initialized())
    return false;
  for (const auto& shortcut_id : shortcut_ids) {
    auto it(guid_map_.find(shortcut_id));
    if (it != guid_map_.end()) {
      shortcuts_map_.erase(it->second);
      guid_map_.erase(it);
    }
  }
  for (ShortcutsBackendObserver& observer : observer_list_)
    observer.OnShortcutsChanged();
  return no_db_access_ ||
         db_runner_->PostTask(
             FROM_HERE,
             base::BindOnce(
                 base::IgnoreResult(&ShortcutsDatabase::DeleteShortcutsWithIDs),
                 db_.get(), shortcut_ids));
}

bool ShortcutsBackend::DeleteShortcutsWithURL(const GURL& url,
                                              bool exact_match) {
  const std::string& url_spec = url.spec();
  ShortcutsDatabase::ShortcutIDs shortcut_ids;
  for (auto it(guid_map_.begin()); it != guid_map_.end();) {
    if (exact_match ? (it->second->second.match_core.destination_url == url)
                    : base::StartsWith(
                          it->second->second.match_core.destination_url.spec(),
                          url_spec, base::CompareCase::SENSITIVE)) {
      shortcut_ids.push_back(it->first);
      shortcuts_map_.erase(it->second);
      guid_map_.erase(it++);
    } else {
      ++it;
    }
  }
  for (ShortcutsBackendObserver& observer : observer_list_)
    observer.OnShortcutsChanged();
  return no_db_access_ ||
         db_runner_->PostTask(
             FROM_HERE,
             base::BindOnce(
                 base::IgnoreResult(&ShortcutsDatabase::DeleteShortcutsWithURL),
                 db_.get(), url_spec));
}

bool ShortcutsBackend::DeleteAllShortcuts() {
  if (!initialized())
    return false;
  shortcuts_map_.clear();
  guid_map_.clear();
  for (ShortcutsBackendObserver& observer : observer_list_)
    observer.OnShortcutsChanged();
  return no_db_access_ ||
         db_runner_->PostTask(
             FROM_HERE,
             base::BindOnce(
                 base::IgnoreResult(&ShortcutsDatabase::DeleteAllShortcuts),
                 db_.get()));
}

bool ShortcutsBackend::DeleteOldShortcuts() {
  ShortcutsDatabase::ShortcutIDs shortcut_ids;
  const base::Time now(base::Time::Now());
  for (const auto& guid_pair : guid_map_) {
    if (now - guid_pair.second->second.last_access_time >
        base::Days(history::HistoryBackend::kExpireDaysThreshold)) {
      shortcut_ids.push_back(guid_pair.first);
    }
  }
  UMA_HISTOGRAM_COUNTS_10000("ShortcutsProvider.OldEntryDeletions.OnInit",
                             shortcut_ids.size());
  return DeleteShortcutsWithIDs(shortcut_ids);
}
