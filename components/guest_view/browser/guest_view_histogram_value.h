// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_HISTOGRAM_VALUE_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_HISTOGRAM_VALUE_H_

namespace guest_view {

// The type of guest view created. Used for metrics purposes.
// This is technically a layering violation (guest view shouldn't know about
// the individual implementations), but it's essentially an opaque bit to the
// guest view system.
// Entries should never be reordered or reused.
enum class GuestViewHistogramValue {
  // An invalid type, used for testing only.
  kInvalid = 0,

  // The <webview> tag, used by webui and platform apps. Note: This is also
  // used for <controlledframe> tags, since they are built on top of webview's
  // implementation.
  kWebView,

  // A guestview used by the PDF viewer and Quick Office.
  kMimeHandler,

  // A guestview used for "inline" extension options pages (shown on the
  // chrome://extensions page).
  kExtensionOptions,

  // A guestview used to embed platform app content inside other platform apps.
  kAppView,

  kMaxValue = kAppView,
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_HISTOGRAM_VALUE_H_
