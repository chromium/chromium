// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_LOGGING_UTILS_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_LOGGING_UTILS_H_

#include "base/time/time.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

struct AutocompleteMatch;

namespace omnibox {

// Logs metrics related to the time elapsed from when the user focused the
// omnibox until a suggestion is opened/acted upon.
void LogFocusToOpenTime(
    base::TimeDelta elapsed,
    bool is_zero_prefix,
    ::metrics::OmniboxEventProto::PageClassification page_classification,
    const AutocompleteMatch& match,
    size_t action_index);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_LOGGING_UTILS_H_
