// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides public definitions.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_PUBLIC_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_PUBLIC_H_

namespace contextual_search {

// The name of our default field trial.
extern const char kContextualSearchFieldTrialName[];

// The name of the variations parameter we use for the Coca Integration param.
extern const char kContextualCardsVersionParamName[];

// The version of the Contextual Cards API that we want to invoke.

// Support of entities with thumnail and caption.
extern const int kContextualCardsEntityIntegration;
// Support of quick actions in the Bar, e.g. dial a phone number.
extern const int kContextualCardsQuickActionsIntegration;
// Support of non-linkified web URLs for quick navigation.
extern const int kContextualCardsUrlActionsIntegration;
// Support of dictionary definitions in the bar.
extern const int kContextualCardsDefinitionsIntegration;
// Support of translations in the bar as part of the resolve request.
extern const int kContextualCardsTranslationsIntegration;
// Support of unlimited cards with diagnostics enabled, for development.
extern const int kContextualCardsDiagnosticIntegration;

// Can be mixed in with one of the above.
extern const int kContextualCardsSimplifiedServerMixin;
extern const char kContextualCardsSimplifiedServerMixinChar[];

// String form of kContextualCardsSimplifiedServerMixin +
// kContextualCardsDiagnosticIntegration.
extern const char kContextualCardsSimplifiedServerWithDiagnosticChar[];

// Longpress resolve variations:
extern const char kLongpressResolveParamName[];
extern const char kLongpressResolveHideOnScroll[];
extern const char kLongpressResolvePrivacyAggressive[];
extern const char kLongpressResolvePreserveTap[];

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_PUBLIC_H_
