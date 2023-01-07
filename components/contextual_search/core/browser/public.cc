// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/core/browser/public.h"

namespace contextual_search {

const char kContextualSearchFieldTrialName[] = "ContextualSearch";

// Contextual Cards variations and integration Api settings.
const char kContextualCardsVersionParamName[] = "contextual_cards_version";

// The version of the Contextual Cards API that we want to invoke.
const int kContextualCardsEntityIntegration = 1;
const int kContextualCardsQuickActionsIntegration = 2;
const int kContextualCardsUrlActionsIntegration = 3;
const int kContextualCardsDefinitionsIntegration = 4;
const int kContextualCardsTranslationsIntegration = 5;

// For development.
const int kContextualCardsDiagnosticIntegration = 99;

// Mixin values. These are a bit mask * 100:
const int kSimplifiedServerDeprecatedMixin = 100;
const int kContextualCardsServerDebugMixin = 200;

}  // namespace contextual_search
