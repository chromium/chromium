// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_PLACEHOLDER_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_PLACEHOLDER_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

class OmniboxController;
class TemplateURLService;

namespace omnibox {

// Returns true if the default search engine placeholder should be the
// "Ask <search engine> or type a URL" variant rather than "Search <search
// engine> or type a URL", i.e. when the AIM popup is enabled and Google is the
// default search engine.
bool ShouldUseAskPlaceholderText(
    bool is_aim_popup_enabled,
    const TemplateURLService* template_url_service);

// Computes the appropriate placeholder text for the omnibox of `controller`
// into `out_placeholder_text`. If a special text should be used for
// accessibility value for placeholder, sets it into `out_a11y_placeholder`;
// otherwise sets it to nullopt.
void ComputePlaceholderText(
    OmniboxController& controller,
    std::u16string& out_placeholder_text,
    std::optional<std::u16string>& out_a11y_placeholder);

// Returns true if the placeholder text should be shown.
// Does not consider if there is regular text or if computed placeholder text
// is empty.
bool ShouldShowPlaceholderText(const OmniboxController& controller,
                               bool in_popup_state_transition,
                               bool aim_button_visible,
                               bool aim_hint_currently_shown);

bool ShouldUseDimPlaceholderColor(OmniboxController& controller);

// Returns true if the AIM placeholder text should be installed instead of the
// DSE placeholder text.
bool ShouldInstallAimPlaceholderText(OmniboxController& controller);

// Returns true if `text` is the AIM placeholder text for the omnibox of
// `controller`.
bool IsAimPlaceholderText(OmniboxController& controller,
                          std::u16string_view text);

// Returns true if the Contextual Tasks placeholder text should be installed
// instead of the DSE placeholder text.
bool ShouldInstallContextualTasksPlaceholderText(
    const OmniboxController& controller);

// Returns true if the AIM hint was shown too often already.
bool AreAimHintImpressionLimitsReached(const OmniboxController& controller,
                                       bool aim_hint_currently_shown);

void RecordAimHintImpression(OmniboxController& controller);

}  // namespace omnibox

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_PLACEHOLDER_UTIL_H_
