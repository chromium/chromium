// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal.h"

#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/base/l10n/l10n_util.h"

OmniboxPedal::LabelStrings::LabelStrings(int id_hint,
                                         int id_hint_short,
                                         int id_suggestion_contents)
    : hint(l10n_util::GetStringUTF16(id_hint)),
      hint_short(l10n_util::GetStringUTF16(id_hint_short)),
      suggestion_contents(l10n_util::GetStringUTF16(id_suggestion_contents)) {}

OmniboxPedal::OmniboxPedal(OmniboxPedal::LabelStrings strings)
    : strings_(strings) {}

OmniboxPedal::~OmniboxPedal() {}

const OmniboxPedal::LabelStrings& OmniboxPedal::GetLabelStrings() const {
  return strings_;
}

bool OmniboxPedal::IsNavigation() const {
  return !url_.is_empty();
}

const GURL& OmniboxPedal::GetNavigationUrl() const {
  return url_;
}

bool OmniboxPedal::ShouldExecute(bool button_pressed) const {
  const auto mode = OmniboxFieldTrial::GetPedalSuggestionMode();
  return (mode == OmniboxFieldTrial::PedalSuggestionMode::DEDICATED) ||
         (mode == OmniboxFieldTrial::PedalSuggestionMode::IN_SUGGESTION &&
          button_pressed);
}

bool OmniboxPedal::ShouldPresentButton() const {
  return OmniboxFieldTrial::GetPedalSuggestionMode() ==
         OmniboxFieldTrial::PedalSuggestionMode::IN_SUGGESTION;
}

void OmniboxPedal::Execute(OmniboxPedal::ExecutionContext& context) const {
  DCHECK(IsNavigation());
  OpenURL(context, url_);
}

bool OmniboxPedal::IsTriggerMatch(const base::string16& match_text) const {
  return triggers_.find(match_text) != triggers_.end();
}

void OmniboxPedal::OpenURL(OmniboxPedal::ExecutionContext& context,
                           const GURL& url) const {
  context.controller_.OnAutocompleteAccept(
      url, WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_GENERATED,
      AutocompleteMatchType::PEDAL, context.match_selection_timestamp_);
}
