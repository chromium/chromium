// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_COMMON_GUEST_VIEW_FEATURES_H_
#define COMPONENTS_GUEST_VIEW_COMMON_GUEST_VIEW_FEATURES_H_

#include "base/feature_list.h"

namespace guest_view {

// Kill switch for converting <webview> autosize sizes between the embedder's
// logical and physical pixels. When enabled (the default), autosize constraints
// (e.g. a <webview>'s minwidth/maxwidth attributes, in CSS px) are converted to
// physical pixels using the embedder's zoom, and re-converted when that zoom
// changes. The resulting autosized size is reported back in logical pixels.
// Remove in M161 or later.
BASE_DECLARE_FEATURE(kGuestViewAutoSizeInLogicalPixels);

// Kill switch for matching a <webview> guest's zoom to its embedder's after
// the guest navigates. When enabled (the default), WebViewGuest and
// SlimWebViewGuest re-apply the embedder's zoom after each main-frame
// navigation (the zoom set when the guest attaches can't take effect before it
// has navigated), unless the page set an explicit zoom with setZoom(), after
// which the guest no longer follows the embedder's zoom. Remove in M161 or
// later.
BASE_DECLARE_FEATURE(kGuestViewMatchEmbedderZoomAfterNavigation);

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_COMMON_GUEST_VIEW_FEATURES_H_
