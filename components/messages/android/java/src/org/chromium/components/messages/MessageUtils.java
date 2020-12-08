// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.chromium.ui.util.AccessibilityUtil;

/**
 * A util class designed for messages.
 */
public class MessageUtils {
    private static AccessibilityUtil sUtil;

    /**
     * @return True if a11y is enabled.
     */
    public static boolean isA11yEnabled() {
        assert sUtil != null;
        return sUtil.isAccessibilityEnabled();
    }

    /**
     * Set an {@link AccessibilityUtil} to know if a11y is enabled.
     * Should set null when message infra is destroyed.
     * @param util The {@link AccessibilityUtil} instance.
     */
    public static void setAccessibilityUtil(AccessibilityUtil util) {
        sUtil = util;
    }
}
