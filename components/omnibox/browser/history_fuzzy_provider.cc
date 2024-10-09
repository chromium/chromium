// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_fuzzy_provider.h"

#include <functional>
#include <memory>
#include <ostream>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/bookmark_provider.h"
#include "components/omnibox/browser/history_quick_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/url_formatter/elide_url.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "url/gurl.h"

namespace {

// Histogram names for measuring sub-provider match conversion efficacy.
// Reminder in case other sub-providers or metrics are added: update
// the `Omnibox.HistoryFuzzy.MatchConversion` entry in histograms.xml.
const char kMetricMatchConversionHistoryQuick[] =
    "Omnibox.HistoryFuzzy.MatchConversion.HistoryQuick";
const char kMetricMatchConversionBookmark[] =
    "Omnibox.HistoryFuzzy.MatchConversion.Bookmark";

// Histogram name for time spent on the fuzzy search portion of provider time.
const char kMetricSearchDuration[] = "Omnibox.HistoryFuzzy.SearchDuration";

// Histogram name for whether a presented fuzzy match was the one taken by the
// user at the moment a match was opened.
const char kMetricPrecision[] = "Omnibox.HistoryFuzzy.Precision";

// This cap ensures the search trie will not grow without bound. Up to half
// the total capacity may be filled at startup from loaded significant URLs.
// The enforced limit may be further constrained by
// `MaxNumHQPUrlsIndexedAtStartup`.
constexpr int kMaxTerminalCount = 256;

// This utility function reduces a URL to the most meaningful and likely part
// of the hostname to be matched against, i.e. the domain, the URL's TLD+1.
// May return an empty string if the given URL is not a good candidate for
// meaningful domain name matching.
std::u16string UrlDomainReduction(const GURL& url) {
  std::u16string url_host;
  std::u16string url_domain;
  url_formatter::SplitHost(url, &url_host, &url_domain, nullptr);
  return url_domain;
}

// This utility function prepares input text for fuzzy matching, or returns
// an empty string in cases unlikely to be worth a fuzzy matching search.
// Note, this is intended to be a fast way to improve matching and eliminate
// likely-unfruitful searches. It could make use of `SplitHost` as above, or
// `url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains`,
// which uses `FormatUrlWithAdjustments` under the hood, but all that URL
// processing for input text that may not even be a URL seems like overkill,
// so this simple direct method is used instead.
std::u16string ReduceInputTextForMatching(const std::u16string& input) {
  constexpr size_t kMaximumFuzzyMatchInputLength = 24;
  constexpr size_t kPathCharacterCountToStopSearch = 6;
  constexpr size_t kPostDotCharacterCountHintingSubdomain = 4;

  // Long inputs are not fuzzy matched; doing so could be costly, and the
  // length of input itself is a signal that it may not have been typed but
  // simply pasted or edited in place.
  if (input.length() > kMaximumFuzzyMatchInputLength) {
    return std::u16string();
  }

  // Spaces hint that the input may be a search, not a URL.
  if (input.find(u' ') != std::u16string::npos) {
    return std::u16string();
  }

  // Inputs containing anything that looks like a scheme are a hint that this
  // is an existing URL or an edit that's likely to be handled deliberately,
  // not a messy human input that may need fuzzy matching.
  if (input.find(u"://") != std::u16string::npos) {
    return std::u16string();
  }

  std::u16string remaining;
  // While typing a URL, the user may typo the domain but then continue on to
  // the path; keeping input up to the path separator keeps the window open
  // for fuzzy matching the domain as they continue to type, but we don't want
  // to keep it open forever (doing so could result in potentially sticky false
  // positives).
  size_t index = input.find(u'/');
  if (index != std::u16string::npos) {
    if (index + kPathCharacterCountToStopSearch < input.length()) {
      // User has moved well beyond typing domain and hasn't taken any fuzzy
      // suggestions provided so far, and they won't get better, so we can
      // save compute and suggestion results space by stopping the search.
      return std::u16string();
    }
    remaining = input.substr(0, index);
  } else {
    remaining = input;
  }

  index = remaining.find(u'.');
  if (index != std::u16string::npos &&
      index + kPostDotCharacterCountHintingSubdomain < remaining.length()) {
    // Keep input with dot if near the end (within range of .com, .org, .edu).
    // With a dot earlier in the string, the user might be typing a subdomain
    // and we only have the TLD+1 stored in the trie, so skip the dot and match
    // against the remaining text. This may be helpful in common cases like
    // typing an unnecessary "www." before the domain name.
    remaining = remaining.substr(index + 1);
  }

  return remaining;
}

}  // namespace

namespace fuzzy {

Edit::Edit(Kind kind, size_t at, char16_t new_char)
    : kind(kind), new_char(new_char), at(at) {}

void Edit::ApplyTo(std::u16string& text) const {
  switch (kind) {
    case Kind::DELETE: {
      text.erase(at, 1);
      break;
    }
    case Kind::INSERT: {
      text.insert(at, 1, new_char);
      break;
    }
    case Kind::REPLACE: {
      text[at] = new_char;
      break;
    }
    case Kind::TRANSPOSE: {
      text[at] = text[at + 1];
      text[at + 1] = new_char;
      break;
    }
    case Kind::KEEP:
    default: {
      NOTREACHED_IN_MIGRATION();
      break;
    }
  }
}

Correction Correction::WithEdit(Edit edit) const {
  DCHECK(edit_count < Correction::kMaxEdits);
  Correction correction = *this;
  correction.edits[edit_count] = edit;
  correction.edit_count++;
  return correction;
}

void Correction::ApplyTo(std::u16string& text) const {
  size_t i = edit_count;
  while (i > 0) {
    i--;
    edits[i].ApplyTo(text);
  }
}

Node::Node() = default;

Node::Node(Node&&) = default;

Node::~Node() = default;

void Node::Insert(const std::u16string& text, size_t text_index) {
  if (text_index >= text.length()) {
    relevance_total += 1 - relevance;
    relevance = 1;
    return;
  }
  std::unique_ptr<Node>& node = next[text[text_index]];
  if (!node) {
    node = std::make_unique<Node>();
  }
  relevance_total -= node->relevance_total;
  node->Insert(text, text_index + 1);
  relevance_total += node->relevance_total;
}

void Node::Delete(const std::u16string& text, size_t text_index) {
  if (text_index < text.length()) {
    auto it = next.find(text[text_index]);
    if (it != next.end()) {
      Node* const node = it->second.get();
      relevance_total -= node->relevance_total;
      node->Delete(text, text_index + 1);
      if (node->relevance_total == 0) {
        next.erase(it);
      } else {
        relevance_total += node->relevance_total;
      }
    }
  } else {
    relevance_total -= relevance;
    relevance = 0;
  }
}

void Node::Clear() {
  next.clear();
}

bool Node::FindCorrections(const std::u16string& text,
                           ToleranceSchedule tolerance_schedule,
                           std::vector<Correction>& corrections) const {
  DCHECK(corrections.empty());
  DCHECK(tolerance_schedule.limit <= Correction::kMaxEdits);

  if (text.length() == 0) {
    return true;
  }

  // A utility class to track search progression.
  struct Step {
    // Walks through trie.
    raw_ptr<const Node> node;

    // Edit distance.
    int distance;

    // Advances through input text. This effectively tells how much of the
    // input has been consumed so far, regardless of output text length.
    size_t index;

    // Length of corrected text. This tells how long the output string will
    // be, regardless of input text length. It is independent of `index`
    // because corrections are not only 1:1 replacements but may involve
    // insertions or deletions as well.
    int length;

    // Backtracking data to enable text correction (from end of string back
    // to beginning, i.e. correction chains are applied in reverse).
    Correction correction;

    // std::priority_queue keeps the greatest element on top, so we want this
    // operator implementation to make bad steps less than good steps.
    // Prioritize minimum distance, with index and length to break ties.
    // The first found solutions are best, and fastest in common cases
    // near input on trie.
    bool operator<(const Step& rhs) const {
      if (distance > rhs.distance) {
        return true;
      } else if (distance == rhs.distance) {
        if (index < rhs.index) {
          return true;
        } else if (index == rhs.index) {
          return length < rhs.length;
        }
      }
      return false;
    }
  };

  std::priority_queue<Step> pq;
  pq.push({this, 0, 0, 0, Correction()});

  Step best{nullptr, INT_MAX, SIZE_MAX, INT_MAX, Correction()};

  // Find and return all equally-distant results as soon as distance increases
  // beyond that of first found results. Length is also considered to
  // avoid producing shorter substring texts.
  while (!pq.empty() && pq.top().distance <= best.distance) {
    Step step = pq.top();
    pq.pop();
    // Strictly greater should not be possible for this comparison.
    if (step.index >= text.length()) {
      if (step.distance == 0) {
        // Ideal common case, full input on trie with no correction required.
        // Because search is directed by priority_queue, we get here before
        // generating any corrections (straight line to goal is shortest path).
        DCHECK(corrections.empty());
        return true;
      }
      // Check `length` to keep longer results. Without this, we could end up
      // with shorter substring corrections (e.g. both "was" and "wash").
      // It may not be necessary to do this if priority_queue keeps results
      // optimal or returns a first best result immediately.
      DCHECK(best.distance == INT_MAX || step.distance == best.distance);
      if (step.distance < best.distance || step.length > best.length) {
        best = std::move(step);
        corrections.clear();
        // Dereference is safe because nonzero distance implies presence of
        // nontrivial correction.
        corrections.emplace_back(best.correction);
      } else {
        // Equal distance.
        // Strictly greater should not be possible for this comparison.
        if (step.length >= best.length) {
          // Dereference is safe because this is another equally
          // distant correction, necessarily discovered after the first.
          corrections.emplace_back(step.correction);
        }
#if DCHECK_ALWAYS_ON
        std::u16string corrected = text;
        step.correction.ApplyTo(corrected);
        DCHECK_EQ(corrected.length(), static_cast<size_t>(step.length))
            << corrected;
#endif
      }
      continue;
    }
    int tolerance = tolerance_schedule.ToleranceAt(step.index);
    if (step.distance < tolerance) {
      // Delete
      pq.push(
          {step.node, step.distance + 1, step.index + 1, step.length,
           step.correction.WithEdit({Edit::Kind::DELETE, step.index, '_'})});
    }
    for (const auto& entry : step.node->next) {
      const char16_t step_text_char = text[step.index];
      if (entry.first == step_text_char) {
        // Keep
        pq.push({entry.second.get(), step.distance, step.index + 1,
                 step.length + 1, step.correction});
      } else if (step.distance < tolerance) {
        // Insert
        pq.push({entry.second.get(), step.distance + 1, step.index,
                 step.length + 1,
                 step.correction.WithEdit(
                     {Edit::Kind::INSERT, step.index, entry.first})});

        // Replace. Note, we do not replace at the same position as a previous
        // insertion because doing so could produce unnecessary duplicates.
        const Edit& step_edit =
            step.correction.edit_count > 0
                ? step.correction.edits[step.correction.edit_count - 1]
                : Edit(Edit::Kind::KEEP, 0, '_');

        if (step_edit.kind != Edit::Kind::INSERT ||
            step_edit.at != step.index) {
          pq.push({entry.second.get(), step.distance + 1, step.index + 1,
                   step.length + 1,
                   step.correction.WithEdit(
                       {Edit::Kind::REPLACE, step.index, entry.first})});
        }

        // Transpose. Look ahead cost can be balanced by faster
        // advancement through input text resulting in shorter search.
        if (text.size() > step.index + 1 &&
            text[step.index + 1] == entry.first) {
          const auto it = entry.second->next.find(step_text_char);
          if (it != entry.second->next.end()) {
            pq.push({it->second.get(), step.distance + 1, step.index + 2,
                     step.length + 2,
                     step.correction.WithEdit(
                         {Edit::Kind::TRANSPOSE, step.index, step_text_char})});
          }
        }
      }
    }
  }

  return false;
}

size_t Node::EstimateMemoryUsage() const {
  size_t res = 0;
  res += base::trace_event::EstimateMemoryUsage(next);
  return res;
}

int Node::TerminalCount() const {
  // This works as long as `relevance` values mark terminals with 1 and
  // non-terminals with 0; see `Insert()`.
  return relevance_total;
}

// This task class loads URLs considered significant according to
// `HistoryDatabase::InitURLEnumeratorForSignificant` but there's nothing
// special about that implementation; we may do something different for
// fuzzy matching. The goal in general is to load and keep a reasonably sized
// set of likely relevant host names for fast fuzzy correction.
class LoadSignificantUrls : public history::HistoryDBTask {
 public:
  using Callback = base::OnceCallback<void(Node)>;

  explicit LoadSignificantUrls(Callback callback)
      : callback_(std::move(callback)) {}
  ~LoadSignificantUrls() override = default;

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    history::URLDatabase::URLEnumerator enumerator;
    if (db && db->InitURLEnumeratorForSignificant(&enumerator)) {
      history::URLRow row;
      // The `MaxNumHQPUrlsIndexedAtStartup` dependency here is to ensure
      // that we keep a lower cap for mobile; it's much higher on desktop.
      // Note the divide, which ensures at least half the capacity will be kept
      // for later visited domains. `GetNextUrl` takes the most significant
      // URLs from the database (enumerator order) and duplicates won't count.
      const int max_terminal_count =
          std::min(OmniboxFieldTrial::MaxNumHQPUrlsIndexedAtStartup(),
                   kMaxTerminalCount) /
          2;
      while (enumerator.GetNextURL(&row) &&
             node_.TerminalCount() < max_terminal_count) {
        node_.Insert(UrlDomainReduction(row.url()), 0);
      }
    }
    return true;
  }

  void DoneRunOnMainThread() override {
    std::move(callback_).Run(std::move(node_));
  }

 private:
  Node node_;
  Callback callback_;
};

}  // namespace fuzzy

// static
void HistoryFuzzyProvider::RecordOpenMatchMetrics(
    const AutocompleteResult& result,
    const AutocompleteMatch& match_opened) {
  if (base::ranges::any_of(result, [](const AutocompleteMatch& match) {
        return match.provider && match.provider->type() ==
                                     AutocompleteProvider::TYPE_HISTORY_FUZZY;
      })) {
    const bool opened_fuzzy_match = match_opened.provider->type() ==
                                    AutocompleteProvider::TYPE_HISTORY_FUZZY;
    UMA_HISTOGRAM_BOOLEAN(kMetricPrecision, opened_fuzzy_match);
  }
}

HistoryFuzzyProvider::HistoryFuzzyProvider(AutocompleteProviderClient* client)
    : HistoryProvider(AutocompleteProvider::TYPE_HISTORY_FUZZY, client) {
  // Set up tunable parameters. These can be used to affect fuzzy matching
  // behavior and performance. Note, we use different `min_input_length_` values
  // depending on desktop versus mobile platforms, determined by experiment.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  min_input_length_ = 5;
#else
  min_input_length_ = 3;
#endif
  // These initial penalty values produce good results for most inputs:
  // Using 10% reasonably took a 1334 relevance match down to 1200,
  // but was harmful to HQP suggestions: as soon as a '.' was
  // appended, a bunch of ~800 navsuggest results overtook a better
  // HQP result that was bumped down to ~770. Using 5% lets this
  // result compete in the navsuggest range.
  penalty_low_ = 5;
  penalty_high_ = 5;
  // The default value of zero means "no taper", and only the lowest penalty
  // will be applied.
  penalty_taper_length_ = 0;

  // In tests, history service is null and doesn't need to be observed.
  if (client->GetHistoryService()) {
    history_service_observation_.Observe(client->GetHistoryService());
    client->GetHistoryService()->ScheduleDBTask(
        FROM_HERE,
        std::make_unique<fuzzy::LoadSignificantUrls>(
            base::BindOnce(&HistoryFuzzyProvider::OnUrlsLoaded,
                           weak_ptr_factory_.GetWeakPtr())),
        &task_tracker_);
  }
}

void HistoryFuzzyProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  TRACE_EVENT0("omnibox", "HistoryFuzzyProvider::Start");
  matches_.clear();
  if (input.IsZeroSuggest() ||
      input.type() == metrics::OmniboxInputType::EMPTY) {
    return;
  }

  // Note this will always return early when bypassing for low-end devices;
  // see comment in constructor.
  if (!urls_loaded_event_.IsSignaled()) {
    return;
  }

  autocomplete_input_ = input;

  // Fuzzy matching intends to correct quick typos, and because it may involve
  // a compute intensive search, some conditions are checked to bypass this
  // provider early. When the cursor is moved from the end of input string,
  // user may have slowed down to edit manually.
  if (autocomplete_input_.cursor_position() ==
      autocomplete_input_.text().length()) {
    DoAutocomplete();
  }
}

size_t HistoryFuzzyProvider::EstimateMemoryUsage() const {
  size_t res = HistoryProvider::EstimateMemoryUsage();
  res += base::trace_event::EstimateMemoryUsage(autocomplete_input_);
  res += base::trace_event::EstimateMemoryUsage(root_);
  return res;
}

HistoryFuzzyProvider::~HistoryFuzzyProvider() = default;

void HistoryFuzzyProvider::DoAutocomplete() {
  constexpr fuzzy::ToleranceSchedule kToleranceSchedule = {
      .start_index = 2,
      .step_length = 4,
      .limit = 3,
  };

  const std::u16string& text =
      ReduceInputTextForMatching(autocomplete_input_.text());
  const size_t input_length = text.length();
  // Note: We can always return if `input_length` is zero, but
  // `min_input_length_` is an experimental parameter for more control.
  // So the second condition can be cleaned up if not needed, but
  // the first condition should be kept regardless.
  if (input_length == 0) {
    return;
  }
  if (input_length < min_input_length_) {
    return;
  }
  std::vector<fuzzy::Correction> corrections;
  const base::TimeTicks time_start = base::TimeTicks::Now();
  root_.FindCorrections(text, kToleranceSchedule, corrections);
  const base::TimeTicks time_end = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES(kMetricSearchDuration, time_end - time_start);
  if (!corrections.empty()) {
    // Relevance ranges are nuanced enough that this should be kept reasonably
    // simple, but the experience of the feature is sensitive to the penalty so
    // we support a range from highest penalty on short inputs to lowest penalty
    // on longer inputs, with a linear taper in between.
    int penalty = penalty_low_;

    // Compute additional penalty for very short inputs.
    if (penalty_taper_length_ > 0) {
      DCHECK_GE(input_length, min_input_length_);
      const size_t extra_length = input_length - min_input_length_;
      if (extra_length <= penalty_taper_length_) {
        DCHECK_GE(penalty_high_, penalty_low_);
        penalty += ((penalty_taper_length_ - extra_length) *
                    (penalty_high_ - penalty_low_)) /
                   penalty_taper_length_;
      }
    }

    // Use of `scoped_refptr` is required here because destructor is private.
    scoped_refptr<HistoryQuickProvider> history_quick_provider =
        new HistoryQuickProvider(client());
    scoped_refptr<BookmarkProvider> bookmark_provider =
        new BookmarkProvider(client());
    int count_history_quick = 0;
    int count_bookmark = 0;
    for (const auto& correction : corrections) {
      std::u16string fixed = text;
      correction.ApplyTo(fixed);

      // Note the `cursor_position` could be changed by insert or delete
      // corrections, but this is easy to adapt since we only fuzzy
      // match when cursor is at end of input; just move to new end.
      DCHECK_EQ(autocomplete_input_.cursor_position(),
                autocomplete_input_.text().length());
      AutocompleteInput corrected_input(
          fixed, fixed.length(),
          autocomplete_input_.current_page_classification(),
          client()->GetSchemeClassifier());

      history_quick_provider->Start(corrected_input, false);
      DCHECK(history_quick_provider->done());
      bookmark_provider->Start(corrected_input, false);
      DCHECK(bookmark_provider->done());

      count_history_quick +=
          AddConvertedMatches(history_quick_provider->matches(), penalty);
      count_bookmark +=
          AddConvertedMatches(bookmark_provider->matches(), penalty);
    }
    if (matches_.size() > provider_max_matches_) {
      // When too many matches are generated, take only the most relevant
      // matches and correct the counts for accurate metrics.
      std::partial_sort(matches_.begin(),
                        matches_.begin() + provider_max_matches_,
                        matches_.end(), AutocompleteMatch::MoreRelevant);
      for (size_t i = provider_max_matches_; i < matches_.size(); i++) {
        DCHECK(matches_[i].provider.get() == history_quick_provider ||
               matches_[i].provider.get() == bookmark_provider)
            << matches_[i].provider->GetName();
        if (matches_[i].provider.get() == history_quick_provider) {
          count_history_quick--;
        } else {
          count_bookmark--;
        }
      }
      matches_.resize(provider_max_matches_);
    }

    for (AutocompleteMatch& match : matches_) {
      match.provider = this;
    }

    RecordMatchConversion(kMetricMatchConversionHistoryQuick,
                          count_history_quick);
    RecordMatchConversion(kMetricMatchConversionBookmark, count_bookmark);
  }
}

int HistoryFuzzyProvider::AddConvertedMatches(const ACMatches& matches,
                                              int penalty) {
  if (matches.empty()) {
    return 0;
  }

  // Take only the most relevant match, to give the best chance of keeping
  // the penalized fuzzy match while reducing risk of possible noise.
  // Note that min_element is used instead of max_element because
  // `AutocompleteMatch::MoreRelevant` reverses standard sort order such that
  // matches with greater relevance are considered less than matches with lesser
  // relevance. For performance reasons, `CompareWithDemoteByType` is not used,
  // so ranking of the final result set will be more nuanced than ranking here.
  ACMatches::const_iterator it = std::min_element(
      matches.begin(), matches.end(), AutocompleteMatch::MoreRelevant);
  CHECK(it != matches.end(), base::NotFatalUntil::M130);
  matches_.push_back(*it);

  // Update match in place. Note, `match.provider` will be reassigned after
  // `DoAutocomplete` because source sub-provider must be kept for metrics.
  AutocompleteMatch& match = matches_.back();

  // It's important that fuzzy matches do not try to become default and inline
  // autocomplete because the input/match-data mismatch can cause problems
  // with user interaction and omnibox text editing; see crbug/1347440.
  match.allowed_to_be_default_match = false;
  match.inline_autocompletion.clear();

  // Apply relevance penalty; all corrections are equal and we only apply this
  // to the most relevant result, so edit distance isn't needed.
  DCHECK_GE(penalty, 0);
  DCHECK_LE(penalty, 100);
  match.relevance -= match.relevance * penalty / 100;

  // Scoring signals are calculated in the history and bookmark providers using
  // the corrected input. These scoring signals are inaccurate for the true
  // input, so clear them to prevent the ml model assigning an
  // artificially high confidence to this suggestion.
  match.scoring_signals.reset();

  return 1;
}

void HistoryFuzzyProvider::OnUrlsLoaded(fuzzy::Node node) {
  root_ = std::move(node);
  urls_loaded_event_.Signal();
}

void HistoryFuzzyProvider::OnURLVisited(
    history::HistoryService* history_service,
    const history::URLRow& url_row,
    const history::VisitRow& new_visit) {
  if (root_.TerminalCount() <
      std::min(OmniboxFieldTrial::MaxNumHQPUrlsIndexedAtStartup(),
               kMaxTerminalCount)) {
    root_.Insert(UrlDomainReduction(url_row.url()), 0);
  }
}

void HistoryFuzzyProvider::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // Note, this implementation is conservative in terms of user privacy; it
  // deletes hosts from the trie if any URL with the given host is deleted.
  if (deletion_info.IsAllHistory()) {
    root_.Clear();
  } else {
    for (const history::URLRow& row : deletion_info.deleted_rows()) {
      root_.Delete(UrlDomainReduction(row.url()), 0);
    }
  }
}

void HistoryFuzzyProvider::RecordMatchConversion(const char* name, int count) {
  base::UmaHistogramExactLinear(
      name, count, AutocompleteResult::kMaxAutocompletePositionValue);
}
