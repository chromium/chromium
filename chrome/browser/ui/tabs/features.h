// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_FEATURES_H_
#define CHROME_BROWSER_UI_TABS_FEATURES_H_

#include "base/feature_list.h"

namespace tabs {

// TODO(346837232) move all flags to this file.

BASE_DECLARE_FEATURE(kSplitTabStrip);

BASE_DECLARE_FEATURE(kScrollableTabStrip);
extern const char kMinimumTabWidthFeatureParameterName[];

BASE_DECLARE_FEATURE(kScrollableTabStripWithDragging);
extern const char kTabScrollingWithDraggingModeName[];

BASE_DECLARE_FEATURE(kScrollableTabStripOverflow);
extern const char kScrollableTabStripOverflowModeName[];

BASE_DECLARE_FEATURE(kTabSearchPositionSetting);

BASE_DECLARE_FEATURE(kTabStripCollectionStorage);

extern bool CanShowTabSearchPositionSetting();

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_FEATURES_H_
