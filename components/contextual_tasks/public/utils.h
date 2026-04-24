// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_UTILS_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_UTILS_H_

#include <memory>
#include <optional>
#include <string>

#include "components/lens/lens_overlay_invocation_source.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"

class GURL;

namespace url_deduplication {
class URLDeduplicationHelper;
}  // namespace url_deduplication

namespace contextual_tasks {

// Creates a URLDeduplicationHelper for comparing URLs. Meant to be used for
// showing the list of distinct URLs associated with a context.
std::unique_ptr<url_deduplication::URLDeduplicationHelper>
CreateURLDeduplicationHelperForContextualTask();

// Returns the URL for the default AI page for a given task.
GURL GetDefaultAimUrl(const std::string& locale,
                      omnibox::ChromeAimEntryPoint entry_point);

// Appends the AIM entry point and Lens invocation source query parameters to
// the given URL, if the entry point is used for AIM zero-state invocations.
GURL AppendAimEntryPointParams(GURL url,
                               omnibox::ChromeAimEntryPoint entry_point);

// Returns the Lens invocation source for the given AIM entry point, if it
// is used for AIM zero-state invocations.
std::optional<lens::LensOverlayInvocationSource>
GetLensInvocationSourceForAimZeroState(
    omnibox::ChromeAimEntryPoint entry_point);

// Checks if the URL has a dark mode override param. Returns nullopt if no param
// is present. If present, returns true for dark mode, false for light mode.
std::optional<bool> GetDarkModeFromUrl(const GURL& url);
}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_UTILS_H_
