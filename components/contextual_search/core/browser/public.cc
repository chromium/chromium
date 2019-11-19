// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/core/browser/public.h"

namespace contextual_search {

const char kContextualSearchFieldTrialName[] = "ContextualSearch";

// Longpress Resolve variations:
const char kLongpressResolveParamName[] = "longpress_resolve_variation";
const char kLongpressResolveHideOnScroll[] = "1";
const char kLongpressResolvePrivacyAggressive[] = "2";
const char kLongpressResolvePreserveTap[] = "3";

// Contextual Cards variations and integration Api settings.
const char kContextualCardsVersionParamName[] = "contextual_cards_version";
// The version of the Contextual Cards API that we want to invoke.
const int kContextualCardsEntityIntegration = 1;
const int kContextualCardsQuickActionsIntegration = 2;
const int kContextualCardsUrlActionsIntegration = 3;
const int kContextualCardsDefinitionsIntegration = 4;
const int kContextualCardsTranslationsIntegration = 5;
const int kContextualCardsDiagnosticIntegration = 99;

const int kContextualCardsSimplifiedServerMixin = 100;
const char kContextualCardsSimplifiedServerMixinChar[] = "100";
const char kContextualCardsSimplifiedServerWithDiagnosticChar[] = "199";

}  // namespace contextual_search
