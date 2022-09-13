// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_trigger_decision.h"

namespace translate {

TranslateTriggerDecision::TranslateTriggerDecision() = default;
TranslateTriggerDecision::TranslateTriggerDecision(
    const TranslateTriggerDecision& other) = default;
TranslateTriggerDecision::~TranslateTriggerDecision() = default;

void TranslateTriggerDecision::PreventAllTriggering() {
  can_auto_translate_ = false;
  can_show_ui_ = false;
  can_auto_href_translate_ = false;
  can_show_href_translate_ui_ = false;
  can_show_predefined_language_translate_ui_ = false;
  can_auto_translate_for_predefined_language_ = false;
}

void TranslateTriggerDecision::PreventAutoTranslate() {
  can_auto_translate_ = false;
}
bool TranslateTriggerDecision::can_auto_translate() const {
  return can_auto_translate_;
}

void TranslateTriggerDecision::PreventShowingUI() {
  can_show_ui_ = false;
}
bool TranslateTriggerDecision::can_show_ui() const {
  return can_show_ui_;
}

void TranslateTriggerDecision::PreventAutoHrefTranslate() {
  can_auto_href_translate_ = false;
}
bool TranslateTriggerDecision::can_auto_href_translate() const {
  return can_auto_href_translate_;
}

void TranslateTriggerDecision::PreventShowingHrefTranslateUI() {
  can_show_href_translate_ui_ = false;
}
bool TranslateTriggerDecision::can_show_href_translate_ui() const {
  return can_show_href_translate_ui_;
}

void TranslateTriggerDecision::PreventShowingPredefinedLanguageTranslateUI() {
  can_show_predefined_language_translate_ui_ = false;
}
bool TranslateTriggerDecision::can_show_predefined_language_translate_ui()
    const {
  return can_show_predefined_language_translate_ui_;
}

void TranslateTriggerDecision::SuppressFromRanker() {
  should_suppress_from_ranker_ = true;
}
bool TranslateTriggerDecision::should_suppress_from_ranker() const {
  return should_suppress_from_ranker_;
}

bool TranslateTriggerDecision::IsTriggeringPossible() const {
  return can_auto_translate_ || can_show_ui_;
}

bool TranslateTriggerDecision::ShouldAutoTranslate() const {
  return can_auto_translate_;
}

bool TranslateTriggerDecision::ShouldShowUI() const {
  return !can_auto_translate_ && can_show_ui_ && !should_suppress_from_ranker_;
}
}  // namespace translate
