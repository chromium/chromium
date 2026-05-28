// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_PREFS_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_PREFS_H_

class PrefRegistrySimple;

namespace personal_context::prefs {

inline constexpr char kPersonalContextInAutofillNoticeShouldBeShown[] =
    "personal_context.autofill.notice_should_be_shown";

inline constexpr char kPersonalContextInAutofillNoticeHasBeenShown[] =
    "personal_context.autofill.notice_has_been_shown";

// Represents the user-visible toggle in Autofill settings. Note that this only
// represents the settings toggle, which is only one of multiple conditions for
// PersonalContext to be enabled. Features that want to consume Context must
// instead check via EnablementService.
inline constexpr char kPersonalContextInAutofillSettingsToggleStatus[] =
    "personal_context.autofill.settings_toggle_status";

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace personal_context::prefs

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_PREFS_H_
