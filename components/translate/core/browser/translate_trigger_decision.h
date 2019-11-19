// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_TRIGGER_DECISION_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_TRIGGER_DECISION_H_

#include <vector>
#include "components/translate/core/browser/translate_browser_metrics.h"

namespace translate {

struct TranslateTriggerDecision {
 public:
  TranslateTriggerDecision();
  TranslateTriggerDecision(const TranslateTriggerDecision& other);
  ~TranslateTriggerDecision();

  void PreventAllTriggering();

  void PreventAutoTranslate();
  bool can_auto_translate() const;

  void PreventShowingUI();
  bool can_show_ui() const;

  void PreventAutoHrefTranslate();
  bool can_auto_href_translate() const;

  void PreventShowingHrefTranslateUI();
  bool can_show_href_translate_ui() const;

  void PreventShowingPredefinedLanguageTranslateUI();
  bool can_show_predefined_language_translate_ui() const;

  void SuppressFromRanker();
  bool should_suppress_from_ranker() const;
  bool IsTriggeringPossible() const;

  bool ShouldAutoTranslate() const;

  // Returns true iff:
  // 1. Showing the UI is disallowed (otherwise it would be chosen over showing
  //    the UI).
  // 2. It's possible to show the UI (language/site not blacklisted, connected
  //    to the internet, etc)
  // 3. Ranker isn't requesting that the UI be suppressed.
  bool ShouldShowUI() const;

  std::vector<TranslateBrowserMetrics::InitiationStatusType>
      initiation_statuses;
  std::vector<int> ranker_events;
  std::string auto_translate_target;
  std::string href_translate_target;
  std::string predefined_translate_target;

 private:
  // These fields are private because they should only be set one way. Filters
  // "blacklist" outcomes, so for example once |can_show_ui_| is set to false,
  // it shouldn't be reset to true.
  bool can_auto_translate_ = true;
  bool can_show_ui_ = true;

  bool can_auto_href_translate_ = true;
  bool can_show_href_translate_ui_ = true;

  // Whether the UI should be shown for a predefined target language
  // which was set via SetPredefinedTargetLanguage call.
  bool can_show_predefined_language_translate_ui_ = true;

  bool should_suppress_from_ranker_ = false;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_TRIGGER_DECISION_H_
