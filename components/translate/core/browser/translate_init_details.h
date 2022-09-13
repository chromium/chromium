// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_INIT_DETAILS_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_INIT_DETAILS_H_

#include "base/time/time.h"
#include "url/gurl.h"

#include "components/translate/core/browser/translate_trigger_decision.h"

namespace translate {

struct TranslateInitDetails {
  TranslateInitDetails();
  TranslateInitDetails(TranslateInitDetails& other);
  ~TranslateInitDetails();

  // The time when this was created
  base::Time time;

  // The URL
  GURL url;

  // Languages translation was initialized with.
  std::string page_language_code;
  std::string target_lang;

  // Decision made during initialize translation.
  TranslateTriggerDecision decision;

  // The UI decision made.
  bool ui_shown;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_INIT_DETAILS_H_
