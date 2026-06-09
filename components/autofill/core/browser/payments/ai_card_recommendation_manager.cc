// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/ai_card_recommendation_manager.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"

namespace autofill::payments {

AiCardRecommendationManager::AiCardRecommendationManager(
    BrowserAutofillManager* browser_autofill_manager)
    : browser_autofill_manager_(CHECK_DEREF(browser_autofill_manager)) {}

AiCardRecommendationManager::~AiCardRecommendationManager() = default;

}  // namespace autofill::payments
