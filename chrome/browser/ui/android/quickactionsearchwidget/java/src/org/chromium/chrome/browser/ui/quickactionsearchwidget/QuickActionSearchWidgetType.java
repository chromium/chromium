// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.quickactionsearchwidget;

import androidx.annotation.IntDef;

/**
 * An enum to specify the size of the Quick Action Search Widget.
 */
@IntDef({QuickActionSearchWidgetType.INVALID, QuickActionSearchWidgetType.SMALL,
        QuickActionSearchWidgetType.MEDIUM, QuickActionSearchWidgetType.DINO})
public @interface QuickActionSearchWidgetType {
    int INVALID = 0;
    int SMALL = 1;
    int MEDIUM = 2;
    int DINO = 3;
}
