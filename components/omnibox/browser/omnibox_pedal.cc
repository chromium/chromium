// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal.h"

#include <algorithm>
#include <cctype>
#include <numeric>

#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/base/l10n/l10n_util.h"

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

OmniboxPedal::TokenSequence::TokenSequence(size_t reserve_size) {
  tokens_.reserve(reserve_size);
}

OmniboxPedal::TokenSequence::TokenSequence(std::vector<int> token_ids) {
  tokens_.reserve(token_ids.size());
  for (int id : token_ids) {
    Add(id);
  }
}

OmniboxPedal::TokenSequence::TokenSequence(OmniboxPedal::TokenSequence&&) =
    default;
OmniboxPedal::TokenSequence::~TokenSequence() = default;

bool OmniboxPedal::TokenSequence::IsFullyConsumed() {
  return WalkToUnconsumedIndexFrom(0) >= Size();
}

size_t OmniboxPedal::TokenSequence::CountUnconsumed() const {
  size_t index = 0;
  size_t count = 0;
  while (index < Size()) {
    if (tokens_[index].link == index) {
      ++count;
      ++index;
    } else {
      index = tokens_[index].link;
    }
  }
  return count;
}

void OmniboxPedal::TokenSequence::Add(int id) {
  tokens_.push_back({id, tokens_.size()});
}

void OmniboxPedal::TokenSequence::ResetLinks() {
  for (size_t i = 0; i < tokens_.size(); ++i) {
    tokens_[i].link = i;
  }
}

bool OmniboxPedal::TokenSequence::Erase(
    const OmniboxPedal::TokenSequence& erase_sequence,
    bool erase_only_once) {
  if (Size() == 0 || erase_sequence.Size() == 0 ||
      erase_sequence.Size() > Size()) {
    return false;
  }
  bool changed = false;
  ptrdiff_t index = ptrdiff_t{Size()} - ptrdiff_t{erase_sequence.Size()};
  while (index >= 0) {
    if (MatchesAt(erase_sequence, index, 0)) {
      // Erase sequence matched by actual removal from container.
      const auto iter = tokens_.begin() + index;
      tokens_.erase(iter, iter + erase_sequence.Size());
      if (erase_only_once) {
        return true;
      }
      changed = true;
      index = std::min(index - 1,
                       ptrdiff_t{Size()} - ptrdiff_t{erase_sequence.Size()});
    } else {
      --index;
    }
  }
  return changed;
}

bool OmniboxPedal::TokenSequence::Consume(
    const OmniboxPedal::TokenSequence& consume_sequence,
    bool consume_only_once) {
  if (Size() == 0 || consume_sequence.Size() == 0 ||
      consume_sequence.Size() > Size()) {
    return false;
  }
  bool changed = false;
  size_t index = WalkToUnconsumedIndexFrom(0);
  const size_t end = 1 + Size() - consume_sequence.Size();
  while (index < end) {
    if (MatchesAt(consume_sequence, index, ~0)) {
      // Erase sequence matched. Remove by updating links to skip.
      tokens_[index].link =
          WalkToUnconsumedIndexFrom(index + consume_sequence.Size());
      if (consume_only_once) {
        return true;
      }
      index = tokens_[index].link;
      changed = true;
    } else {
      // No match. Proceed by single step.
      index = WalkToUnconsumedIndexFrom(index + 1);
    }
  }
  return changed;
}

size_t OmniboxPedal::TokenSequence::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(tokens_);
}

bool OmniboxPedal::TokenSequence::MatchesAt(
    const OmniboxPedal::TokenSequence& sequence,
    size_t index,
    size_t index_mask) const {
  for (const auto& sequence_token : sequence.tokens_) {
    const auto& from_token = tokens_[index];
    if (from_token.id != sequence_token.id ||
        (from_token.link & index_mask) != (index & index_mask)) {
      return false;
    }
    ++index;
  }
  return true;
}

size_t OmniboxPedal::TokenSequence::WalkToUnconsumedIndexFrom(
    size_t from_index) {
  size_t index = from_index;
  while (index < Size()) {
    const size_t link = tokens_[index].link;
    if (link == index) {
      break;
    }
    index = link;
    // Shorten path so that future walks remain near constant time.
    tokens_[from_index].link = link;
  }
  return index;
}

// =============================================================================

OmniboxPedal::LabelStrings::LabelStrings(int id_hint,
                                         int id_suggestion_contents,
                                         int id_accessibility_suffix,
                                         int id_accessibility_hint)
    : hint(l10n_util::GetStringUTF16(id_hint)),
      suggestion_contents(l10n_util::GetStringUTF16(id_suggestion_contents)),
      accessibility_suffix(l10n_util::GetStringUTF16(id_accessibility_suffix)),
      accessibility_hint(l10n_util::GetStringUTF16(id_accessibility_hint)) {}

OmniboxPedal::LabelStrings::LabelStrings() = default;

OmniboxPedal::LabelStrings::LabelStrings(const LabelStrings&) = default;

OmniboxPedal::LabelStrings::~LabelStrings() = default;

// =============================================================================

OmniboxPedal::SynonymGroup::SynonymGroup(bool required,
                                         bool match_once,
                                         size_t reserve_size)
    : required_(required), match_once_(match_once) {
  synonyms_.reserve(reserve_size);
}

OmniboxPedal::SynonymGroup::SynonymGroup(SynonymGroup&&) = default;

OmniboxPedal::SynonymGroup::~SynonymGroup() = default;

OmniboxPedal::SynonymGroup& OmniboxPedal::SynonymGroup::operator=(
    SynonymGroup&&) = default;

bool OmniboxPedal::SynonymGroup::EraseMatchesIn(
    OmniboxPedal::TokenSequence& remaining,
    bool fully_erase) const {
  auto eraser = fully_erase ? &TokenSequence::Erase : &TokenSequence::Consume;
  bool changed = false;
  for (const auto& synonym : synonyms_) {
    if (base::invoke(eraser, remaining, synonym, match_once_)) {
      changed = true;
      if (match_once_) {
        break;
      }
    }
  }
  return changed || !required_;
}

void OmniboxPedal::SynonymGroup::AddSynonym(
    OmniboxPedal::TokenSequence synonym) {
#if DCHECK_IS_ON()
  if (synonyms_.size() > size_t{0}) {
    DCHECK_GE(synonyms_.back().Size(), synonym.Size());
  }
#endif
  synonyms_.push_back(std::move(synonym));
}

size_t OmniboxPedal::SynonymGroup::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(synonyms_);
}

// =============================================================================

namespace base {
namespace trace_event {
size_t EstimateMemoryUsage(const OmniboxPedal::LabelStrings& self) {
  size_t total = 0;
  total += base::trace_event::EstimateMemoryUsage(self.hint);
  total += base::trace_event::EstimateMemoryUsage(self.suggestion_contents);
  total += base::trace_event::EstimateMemoryUsage(self.accessibility_suffix);
  total += base::trace_event::EstimateMemoryUsage(self.accessibility_hint);
  return total;
}
}  // namespace trace_event
}  // namespace base

// =============================================================================

OmniboxPedal::OmniboxPedal(OmniboxPedalId id, LabelStrings strings, GURL url)
    : id_(id), strings_(strings), url_(url) {}

OmniboxPedal::~OmniboxPedal() {}

const OmniboxPedal::LabelStrings& OmniboxPedal::GetLabelStrings() const {
  return strings_;
}

void OmniboxPedal::SetLabelStrings(const base::Value& ui_strings) {
  DCHECK(ui_strings.is_dict());
  // The pedal_processor tool ensures that this dictionary is either omitted,
  //  or else included with all these keys populated.
  ui_strings.FindKey("button_text")->GetAsString(&strings_.hint);
  ui_strings.FindKey("description_text")
      ->GetAsString(&strings_.suggestion_contents);
  ui_strings.FindKey("spoken_button_focus_announcement")
      ->GetAsString(&strings_.accessibility_hint);
  ui_strings.FindKey("spoken_suggestion_description_suffix")
      ->GetAsString(&strings_.accessibility_suffix);
}

bool OmniboxPedal::IsNavigation() const {
  return !url_.is_empty();
}

const GURL& OmniboxPedal::GetNavigationUrl() const {
  return url_;
}

void OmniboxPedal::SetNavigationUrl(const GURL& url) {
  url_ = url;
}

void OmniboxPedal::Execute(OmniboxPedal::ExecutionContext& context) const {
  DCHECK(IsNavigation());
  OpenURL(context, url_);
}

bool OmniboxPedal::IsReadyToTrigger(
    const AutocompleteInput& input,
    const AutocompleteProviderClient& client) const {
  return true;
}

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
// static
const gfx::VectorIcon& OmniboxPedal::GetDefaultVectorIcon() {
  if (OmniboxFieldTrial::IsPedalsDefaultIconColored()) {
    return omnibox::kPedalIcon;
  } else {
    return omnibox::kProductIcon;
  }
}

const gfx::VectorIcon& OmniboxPedal::GetVectorIcon() const {
  return GetDefaultVectorIcon();
}
#endif

void OmniboxPedal::AddSynonymGroup(SynonymGroup&& group) {
  synonym_groups_.push_back(std::move(group));
}

size_t OmniboxPedal::EstimateMemoryUsage() const {
  size_t total = 0;
  total += base::trace_event::EstimateMemoryUsage(url_);
  total += base::trace_event::EstimateMemoryUsage(strings_);
  total += base::trace_event::EstimateMemoryUsage(synonym_groups_);
  return total;
}

bool OmniboxPedal::IsConceptMatch(TokenSequence& match_sequence) const {
  for (const auto& group : synonym_groups_) {
    if (!group.EraseMatchesIn(match_sequence, false))
      return false;
  }
  return match_sequence.IsFullyConsumed();
}

void OmniboxPedal::OpenURL(OmniboxPedal::ExecutionContext& context,
                           const GURL& url) const {
  // destination_url_entered_without_scheme is used to determine whether
  // navigations typed without a scheme and upgraded to HTTPS should fall back
  // to HTTP. The URL might have been entered without a scheme, but pedal
  // destination URLs don't need a fallback so it's fine to pass false here.
  context.controller_.OnAutocompleteAccept(
      url, nullptr, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_GENERATED, AutocompleteMatchType::PEDAL,
      context.match_selection_timestamp_,
      /*destination_url_entered_without_scheme=*/false);
}
