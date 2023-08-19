// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/common/guest_view_constants.h"

namespace guest_view {

// Sizing attributes/parameters.
const char kAttributeAutoSize[] = "autosize";
const char kAttributeMaxHeight[] = "maxheight";
const char kAttributeMaxWidth[] = "maxwidth";
const char kAttributeMinHeight[] = "minheight";
const char kAttributeMinWidth[] = "minwidth";
const char kElementWidth[] = "elementWidth";
const char kElementHeight[] = "elementHeight";
const char kElementSizeIsLogical[] = "elementSizeIsLogical";

// Events.
const char kEventResize[] = "guestViewInternal.onResize";

// Parameters/properties on events.
const char kCode[] = "code";
const char kIsTopLevel[] = "isTopLevel";
const char kNewWidth[] = "newWidth";
const char kNewHeight[] = "newHeight";
const char kOldWidth[] = "oldWidth";
const char kOldHeight[] = "oldHeight";
const char kReason[] = "reason";
const char kUrl[] = "url";
const char kUserGesture[] = "userGesture";

// Initialization parameters.
const char kParameterInstanceId[] = "instanceId";

// Other.
const char kGuestViewManagerKeyName[] = "guest_view_manager";
const int kInstanceIDNone = 0;
const int kDefaultWidth = 300;
const int kDefaultHeight = 300;

}  // namespace guestview
