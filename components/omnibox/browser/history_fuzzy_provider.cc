// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_fuzzy_provider.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <ostream>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/bookmark_provider.h"
#include "components/omnibox/browser/history_quick_provider.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "components/url_formatter/elide_url.h"
#include "url/gurl.h"

namespace {

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
  constexpr size_t kMaximumFuzzyMatchInputLength = 32;
  constexpr size_t kPathCharacterCountToStopSearch = 6;
  constexpr size_t kPostDotCharacterCountHintingSubdomain = 4;

  // Long inputs are not fuzzy matched; doing so could be costly, and the
  // length of input itself is a signal that it may not have been typed but
  // simply pasted or edited in place.
  // TODO(orinj): Consider tracking trie depth for use as maximum here.
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

Correction::Correction(const Correction& other) {
  kind = other.kind;
  at = other.at;
  new_char = other.new_char;
  if (other.next) {
    next = std::make_unique<Correction>(*other.next.get());
  }
}

Correction::Correction(Correction&&) = default;

Correction::Correction(Kind kind, size_t at, char16_t new_char)
    : kind(kind), at(at), new_char(new_char) {}

Correction::Correction(Kind kind,
                       size_t at,
                       char16_t new_char,
                       std::unique_ptr<Correction> next)
    : kind(kind), at(at), new_char(new_char), next(std::move(next)) {}

Correction::~Correction() = default;

void Correction::ApplyTo(std::u16string& text) const {
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
    case Kind::KEEP:
    default: {
      NOTREACHED();
      break;
    }
  }
  if (next) {
    next->ApplyTo(text);
  }
}

std::unique_ptr<Correction> Correction::GetApplicableCorrection() {
  if (kind == Kind::KEEP) {
    // Because this function eliminates KEEP corrections as the chain is built,
    // it doesn't need to work recursively; a single elimination is sufficient.
    DCHECK(!next || next->kind != Kind::KEEP);
    return next ? std::make_unique<Correction>(*next) : nullptr;
  } else {
    // TODO(orinj): Consider a shared ownership model or preallocated pool to
    //  eliminate lots of copy allocations. For now this is kept simple with
    //  direct ownership of the full correction chain.
    return std::make_unique<Correction>(*this);
  }
}

// This operator implementation is for debugging.
std::ostream& operator<<(std::ostream& os, const Correction& correction) {
  os << '{';
  switch (correction.kind) {
    case Correction::Kind::KEEP: {
      os << 'K';
      break;
    }
    case Correction::Kind::DELETE: {
      os << 'D';
      break;
    }
    case Correction::Kind::INSERT: {
      os << 'I';
      break;
    }
    case Correction::Kind::REPLACE: {
      os << 'R';
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
  os << "," << correction.at << "," << static_cast<char>(correction.new_char)
     << "}";
  return os;
}

Node::Node() = default;

Node::Node(Node&&) = default;

Node::~Node() = default;

void Node::Insert(const std::u16string& text, size_t from) {
  if (from >= text.length()) {
    relevance = 1;
    return;
  }
  std::unique_ptr<Node>& node = next[text[from]];
  if (!node) {
    node = std::make_unique<Node>();
  }
  node->Insert(text, from + 1);
}

bool Node::Delete(const std::u16string& text, size_t from) {
  if (from < text.length()) {
    auto it = next.find(text[from]);
    if (it != next.end() && it->second->Delete(text, from + 1)) {
      next.erase(it);
    }
  }
  return next.empty();
}

void Node::Clear() {
  next.clear();
}

bool Node::FindCorrections(const std::u16string& text,
                           ToleranceSchedule tolerance_schedule,
                           std::vector<Correction>& corrections) const {
  DVLOG(1) << "FindCorrections(" << text << ", " << tolerance_schedule.limit
           << ")";
  DCHECK(corrections.empty());

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
    // TODO(orinj): This should be optimized in final algorithm; stop copying.
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
  pq.push({this, 0, 0, 0, {Correction::Kind::KEEP, 0, '_'}});

  Step best{
      nullptr, INT_MAX, SIZE_MAX, INT_MAX, {Correction::Kind::KEEP, 0, '_'}};
  int i = 0;
  // Find and return all equally-distant results as soon as distance increases
  // beyond that of first found results. Length is also considered to
  // avoid producing shorter substring texts.
  while (!pq.empty() && pq.top().distance <= best.distance) {
    i++;
    Step step = pq.top();
    pq.pop();
    DVLOG(1) << i << "(" << step.distance << "," << step.index << ","
             << step.length << "," << step.correction << ")";
    // TODO(orinj): Enforce a tolerance schedule with index versus distance;
    //  this would allow more errors for longer inputs and prevents searching
    //  through long corrections near start of input.
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
        DVLOG(1) << "new best by "
                 << (step.distance < best.distance ? "distance" : "length");
        best = std::move(step);
        corrections.clear();
        // Dereference is safe because nonzero distance implies presence of
        // nontrivial correction.
        corrections.emplace_back(*best.correction.GetApplicableCorrection());
      } else {
        // Equal distance.
        // Strictly greater should not be possible for this comparison.
        if (step.length >= best.length) {
          // Dereference is safe because this is another equally
          // distant correction, necessarily discovered after the first.
          corrections.emplace_back(*step.correction.GetApplicableCorrection());
        }
#if DCHECK_ALWAYS_ON
        std::u16string corrected = text;
        step.correction.GetApplicableCorrection()->ApplyTo(corrected);
        DCHECK_EQ(corrected.length(), static_cast<size_t>(step.length))
            << corrected;
#endif
      }
      continue;
    }
    int tolerance = tolerance_schedule.ToleranceAt(step.index);
    if (step.distance < tolerance) {
      // Delete
      pq.push({step.node,
               step.distance + 1,
               step.index + 1,
               step.length,
               {Correction::Kind::DELETE, step.index, '_',
                step.correction.GetApplicableCorrection()}});
    }
    for (const auto& entry : step.node->next) {
      if (entry.first == text[step.index]) {
        // Keep
        pq.push({entry.second.get(),
                 step.distance,
                 step.index + 1,
                 step.length + 1,
                 {Correction::Kind::KEEP, step.index, '_',
                  step.correction.GetApplicableCorrection()}});
      } else if (step.distance < tolerance) {
        // Insert
        pq.push({entry.second.get(),
                 step.distance + 1,
                 step.index,
                 step.length + 1,
                 {Correction::Kind::INSERT, step.index, entry.first,
                  step.correction.GetApplicableCorrection()}});
        // Replace. Note, we do not replace at the same position as a previous
        // insertion because doing so could produce unnecessary duplicates.
        if (step.correction.kind != Correction::Kind::INSERT ||
            step.correction.at != step.index) {
          pq.push({entry.second.get(),
                   step.distance + 1,
                   step.index + 1,
                   step.length + 1,
                   {Correction::Kind::REPLACE, step.index, entry.first,
                    step.correction.GetApplicableCorrection()}});
        }
      }
    }
  }
  if (!pq.empty()) {
    DVLOG(1) << "quit early on step with distance " << pq.top().distance;
  }
  return false;
}

void Node::Log(std::u16string built) const {
  if (relevance > 0) {
    DVLOG(1) << "  <" << built << ">";
  }
  for (const auto& entry : next) {
    entry.second->Log(built + entry.first);
  }
}

size_t Node::EstimateMemoryUsage() const {
  size_t res = 0;
  res += base::trace_event::EstimateMemoryUsage(next);
  return res;
}

// This task class loads URLs considered significant according to
// `HistoryDatabase::InitURLEnumeratorForSignificant` but there's nothing
// special about that implementation; we may do something different for
// fuzzy matching. The goal in general is to load and keep a reasonably sized
// set of likely relevant host names for fast fuzzy correction.
class LoadSignificantUrls : public history::HistoryDBTask {
 public:
  using Callback = base::OnceCallback<void(Node)>;

  LoadSignificantUrls(base::WaitableEvent* event, Callback callback)
      : wait_event_(event), callback_(std::move(callback)) {
    DVLOG(1) << "LoadSignificantUrls ctor thread "
             << base::PlatformThread::CurrentId();
  }
  ~LoadSignificantUrls() override = default;

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    DVLOG(1) << "LoadSignificantUrls run on db thread "
             << base::PlatformThread::CurrentId() << "; db: " << db;
    history::URLDatabase::URLEnumerator enumerator;
    if (db && db->InitURLEnumeratorForSignificant(&enumerator)) {
      DVLOG(1) << "Got InMemoryDatabase";
      history::URLRow row;
      while (enumerator.GetNextURL(&row)) {
        DVLOG(1) << "url #" << row.id() << ": " << row.url().host();
        node_.Insert(UrlDomainReduction(row.url()), 0);
      }
    } else {
      DVLOG(1) << "No significant InMemoryDatabase";
    }
    return true;
  }

  void DoneRunOnMainThread() override {
    DVLOG(1) << "Done thread " << base::PlatformThread::CurrentId();
    std::move(callback_).Run(std::move(node_));
    wait_event_->Signal();
  }

 private:
  Node node_;
  raw_ptr<base::WaitableEvent> wait_event_;
  Callback callback_;
};

}  // namespace fuzzy

HistoryFuzzyProvider::HistoryFuzzyProvider(AutocompleteProviderClient* client)
    : HistoryProvider(AutocompleteProvider::TYPE_HISTORY_FUZZY, client) {
  history_service_observation_.Observe(client->GetHistoryService());
  client->GetHistoryService()->ScheduleDBTask(
      FROM_HERE,
      std::make_unique<fuzzy::LoadSignificantUrls>(
          &urls_loaded_event_,
          base::BindOnce(&HistoryFuzzyProvider::OnUrlsLoaded,
                         weak_ptr_factory_.GetWeakPtr())),
      &task_tracker_);
}

void HistoryFuzzyProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  TRACE_EVENT0("omnibox", "HistoryFuzzyProvider::Start");
  matches_.clear();
  if (input.focus_type() != OmniboxFocusType::DEFAULT ||
      input.type() == metrics::OmniboxInputType::EMPTY) {
    return;
  }

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
  // TODO(orinj): This schedule may want some measurement and tinkering.
  constexpr fuzzy::ToleranceSchedule kToleranceSchedule = {
      .start_index = 1,
      .step_length = 4,
      .limit = 3,
  };

  const std::u16string& text =
      ReduceInputTextForMatching(autocomplete_input_.text());
  if (text.length() == 0) {
    DVLOG(1) << "Skipping fuzzy for input '" << autocomplete_input_.text()
             << "'";
    return;
  }
  if (text[text.length() - 1] == u'!') {
    if (text == u"log!") {
      DVLOG(1) << "Trie Log: !{";
      root_.Log(std::u16string());
      DVLOG(1) << "}!";
    } else {
      root_.Insert(text.substr(0, text.length() - 1), 0);
    }
  } else {
    std::vector<fuzzy::Correction> corrections;
    DVLOG(1) << "FindCorrections: <" << text << "> ---> ?{";
    if (root_.FindCorrections(text, kToleranceSchedule, corrections)) {
      DVLOG(1) << "Trie contains input; no fuzzy results needed";
    }
    if (!corrections.empty()) {
      // Use of `scoped_refptr` is required here because destructor is private.
      scoped_refptr<HistoryQuickProvider> history_quick_provider =
          new HistoryQuickProvider(client());
      scoped_refptr<BookmarkProvider> bookmark_provider =
          new BookmarkProvider(client());
      for (const auto& correction : corrections) {
        std::u16string fixed = text;
        correction.ApplyTo(fixed);
        DVLOG(1) << ":  " << fixed;
        AddMatchForText(fixed);

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

        AddConvertedMatches(history_quick_provider->matches());
        AddConvertedMatches(bookmark_provider->matches());
      }
    }
    DVLOG(1) << "}?";
  }
}

void HistoryFuzzyProvider::AddMatchForText(std::u16string text) {
  AutocompleteMatch match(this, 10000000, false,
                          AutocompleteMatchType::HISTORY_URL);
  match.contents = std::move(text);
  match.contents_class.push_back(
      {0, AutocompleteMatch::ACMatchClassification::DIM});
  matches_.push_back(std::move(match));
}

void HistoryFuzzyProvider::AddConvertedMatches(const ACMatches& matches) {
  // TODO(orinj): Optimize with move not copy; requires provider change.
  //  Consider taking only the most relevant match.
  for (const auto& original_match : matches) {
    DVLOG(1) << "Converted match: " << original_match.contents;
    matches_.push_back(original_match);

    // Update match in place.
    AutocompleteMatch& match = matches_.back();
    match.provider = this;
    match.inline_autocompletion.clear();
    match.allowed_to_be_default_match = false;
    // TODO(orinj): Determine suitable relevance penalty; it should
    //  likely take into account the edit distance or size of correction.
    //  Using 9/10 reasonably took a 1334 relevance match down to 1200,
    //  but was harmful to HQP suggestions: as soon as a '.' was
    //  appended, a bunch of ~800 navsuggest results overtook a better
    //  HQP result that was bumped down to ~770. Using 95/100 lets this
    //  result compete in the navsuggest range.
    match.relevance = match.relevance * 95 / 100;
    match.contents_class.clear();
    match.contents_class.push_back(
        {0, AutocompleteMatch::ACMatchClassification::DIM});
  }
}

void HistoryFuzzyProvider::OnUrlsLoaded(fuzzy::Node node) {
  root_ = std::move(node);
}

void HistoryFuzzyProvider::OnURLVisited(
    history::HistoryService* history_service,
    ui::PageTransition transition,
    const history::URLRow& row,
    base::Time visit_time) {
  DVLOG(1) << "URL Visit: " << row.url();
  root_.Insert(UrlDomainReduction(row.url()), 0);
}

void HistoryFuzzyProvider::OnURLsDeleted(
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
