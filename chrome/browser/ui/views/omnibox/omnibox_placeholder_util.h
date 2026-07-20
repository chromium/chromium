// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_PLACEHOLDER_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_PLACEHOLDER_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

class LocationBar;

namespace omnibox {

// Given a `location_bar`, computes the appropriate placeholder text
// for it into `out_placeholder_text. If a special text should be used for
// accessibility value for placeholder, sets it into `out_a11y_placeholder`;
// otherwise sets it to nullopt.
void ComputePlaceholderText(
    LocationBar* location_bar,
    std::u16string& out_placeholder_text,
    std::optional<std::u16string>& out_a11y_placeholder);

// Returns true if the placeholder text should be shown.
// Does not consider if there is regular text or if computed placeholder text
// is empty.
bool ShouldShowPlaceholderText(LocationBar* location_bar,
                               bool in_popup_state_transition,
                               bool aim_button_visible,
                               bool aim_hint_currently_shown);

bool ShouldUseDimPlaceholderColor(LocationBar* location_bar);

// Returns true if the AIM placeholder text should be installed instead of the
// DSE placeholder text.
bool ShouldInstallAimPlaceholderText(LocationBar* location_bar);

// Returns true if `text` is the AIM placeholder text for the location bar.
bool IsAimPlaceholderText(LocationBar* location_bar, std::u16string_view text);

// Returns true if the Contextual Tasks placeholder text should be installed
// instead of the DSE placeholder text.
bool ShouldInstallContextualTasksPlaceholderText(LocationBar* location_bar);

// Returns true if the AIM hint was shown too often already.
bool AreAimHintImpressionLimitsReached(LocationBar* location_bar,
                                       bool aim_hint_currently_shown);

void RecordAimHintImpression(LocationBar* location_bar);

}  // namespace omnibox

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_PLACEHOLDER_UTIL_H_
