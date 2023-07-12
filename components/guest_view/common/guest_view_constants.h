// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WebView API.

#ifndef COMPONENTS_GUEST_VIEW_COMMON_GUEST_VIEW_CONSTANTS_H_
#define COMPONENTS_GUEST_VIEW_COMMON_GUEST_VIEW_CONSTANTS_H_

namespace guest_view {

// Sizing attributes/parameters.
extern const char kAttributeAutoSize[];
extern const char kAttributeMaxHeight[];
extern const char kAttributeMaxWidth[];
extern const char kAttributeMinHeight[];
extern const char kAttributeMinWidth[];
extern const char kElementWidth[];
extern const char kElementHeight[];
extern const char kElementSizeIsLogical[];

// Events.
extern const char kEventResize[];

// Parameters/properties on events.
extern const char kCode[];
extern const char kIsTopLevel[];
extern const char kNewWidth[];
extern const char kNewHeight[];
extern const char kOldWidth[];
extern const char kOldHeight[];
extern const char kReason[];
extern const char kUrl[];
extern const char kUserGesture[];

// Initialization parameters.
extern const char kParameterInstanceId[];

// Other.
extern const char kGuestViewManagerKeyName[];
extern const int kInstanceIDNone;
extern const int kDefaultWidth;
extern const int kDefaultHeight;

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_COMMON_GUEST_VIEW_CONSTANTS_H_
