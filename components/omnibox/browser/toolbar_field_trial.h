// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TOOLBAR_FIELD_TRIAL_H_
#define COMPONENTS_OMNIBOX_BROWSER_TOOLBAR_FIELD_TRIAL_H_

#include "base/feature_list.h"

namespace toolbar {
namespace features {

// Feature used to hide the scheme from steady state URLs displayed in the
// toolbar. It is restored during editing.
extern const base::Feature kHideSteadyStateUrlScheme;

// Feature used to hide trivial subdomains from steady state URLs displayed in
// the toolbar. It is restored during editing.
extern const base::Feature kHideSteadyStateUrlTrivialSubdomains;

// Feature used to hide the file scheme from URLs displayed in the toolbar.
// It is restored during editing.
extern const base::Feature kHideFileUrlScheme;

// Feature used to hide the path, query and ref from steady state URLs
// displayed in the toolbar. It is restored during editing.
extern const base::Feature kHideSteadyStateUrlPathQueryAndRef;

// Returns true if either the steady-state elision flag for scheme or the
// #upcoming-ui-features flag is enabled.
bool IsHideSteadyStateUrlSchemeEnabled();

// Returns true if either the steady-state elision flag for trivial
// subdomains or the #upcoming-ui-features flag is enabled.
bool IsHideSteadyStateUrlTrivialSubdomainsEnabled();

// This feature simplifies the security indiciator UI for https:// pages. The
// exact UI treatment is dependent on the parameter 'treatment' which can have
// the following value:
// - 'ev-to-secure': Show the "Secure" chip for pages with an EV certificate.
// - 'secure-to-lock': Show only the lock icon for non-EV https:// pages.
// - 'both-to-lock': Show only the lock icon for all https:// pages.
// - 'keep-secure-chip': Show the old "Secure" chip for non-EV https:// pages.
// The default behavior is the same as 'secure-to-lock'.
extern const base::Feature kSimplifyHttpsIndicator;

// The parameter name which controls the UI treatment.
extern const char kSimplifyHttpsIndicatorParameterName[];

// The different parameter values, described above.
extern const char kSimplifyHttpsIndicatorParameterEvToSecure[];
extern const char kSimplifyHttpsIndicatorParameterSecureToLock[];
extern const char kSimplifyHttpsIndicatorParameterBothToLock[];
extern const char kSimplifyHttpsIndicatorParameterKeepSecureChip[];

}  // namespace features
}  // namespace toolbar

#endif  // COMPONENTS_OMNIBOX_BROWSER_TOOLBAR_FIELD_TRIAL_H_
