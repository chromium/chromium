// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_SEARCHBOX_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_SEARCHBOX_UTILS_H_

#include <optional>

#include "components/lens/lens_overlay_invocation_source.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

// Returns the AimEntryPoint corresponding to the given PageClassification and
// optional session handle.
omnibox::ChromeAimEntryPoint GetAimEntryPoint(
    ::metrics::OmniboxEventProto::PageClassification classification,
    const contextual_search::ContextualSearchSessionHandle* session_handle =
        nullptr);

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_SEARCHBOX_UTILS_H_
