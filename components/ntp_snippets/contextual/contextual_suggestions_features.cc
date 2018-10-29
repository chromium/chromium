// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_suggestions_features.h"

namespace contextual_suggestions {

const base::Feature kContextualSuggestionsAlternateCardLayout{
    "ContextualSuggestionsAlternateCardLayout",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSuggestionsButton{
    "ContextualSuggestionsButton", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSuggestionsIPHReverseScroll{
    "ContextualSuggestionsIPHReverseScroll", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kContextualSuggestionsOptOut{
    "ContextualSuggestionsOptOut", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace contextual_suggestions
