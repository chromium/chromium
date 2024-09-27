// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_PAGE_CLASSIFICATION_FUNCTIONS_H_
#define COMPONENTS_OMNIBOX_BROWSER_PAGE_CLASSIFICATION_FUNCTIONS_H_

#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace omnibox {

// Return true, if supplied page classification is a new tab page.
bool IsNTPPage(::metrics::OmniboxEventProto::PageClassification classification);

// Return true, if supplied page classification is a search results page.
bool IsSearchResultsPage(
    ::metrics::OmniboxEventProto::PageClassification classification);

// Return true, if supplied page classification is neither a new tab page or
// search results page.
bool IsOtherWebPage(
    ::metrics::OmniboxEventProto::PageClassification classification);

// Return true, if supplied page classification is a Lens unimodal, multimodal,
// or contextual searchbox.
bool IsLensSearchbox(
    ::metrics::OmniboxEventProto::PageClassification classification);

// Return true, if supplied page classification is a Chrome Custom Tab.
bool IsCustomTab(
    ::metrics::OmniboxEventProto::PageClassification classification);

// Return true, if supplied page classification is the Android Hub searchbox.
bool IsAndroidHub(
    ::metrics::OmniboxEventProto::PageClassification classification);

// Checks (and asserts in development builds) if Page Classification is
// obsolete and should not be used.
void CheckObsoletePageClass(
    ::metrics::OmniboxEventProto::PageClassification classification);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_PAGE_CLASSIFICATION_FUNCTIONS_H_
