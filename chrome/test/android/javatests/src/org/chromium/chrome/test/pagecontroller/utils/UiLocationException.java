// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import androidx.test.uiautomator.UiObject2;

/**
 * Exception class that represents an unexpected failure when trying to find
 * a UI node.
 */
public class UiLocationException extends IllegalStateException {
    /**
     * Creates a UiLocationException exception when the locator used is not
     * known.
     *
     * @param msg The error message.
     */
    public UiLocationException(String msg) {
        super(msg);
    }

    /**
     * Creates a UiLocationException exception when the locator failed to find
     * any nodes in the root node.
     *
     * @param msg     The error message.
     * @param locator The locator that failed to find any nodes.
     * @param root    The root that the locator searched under, or null if all the nodes were
     *                searched.
     */
    public UiLocationException(String msg, IUi2Locator locator, UiObject2 root) {
        this(msg + " (Locator=" + locator + " in root=" + root + ")");
    }

    /**
     * Creates a UiLocationException exception when the locator failed to find
     * any nodes on the device.
     *
     * @param msg     The error message.
     * @param locator The locator that failed to find any nodes.
     */
    public UiLocationException(String msg, IUi2Locator locator) {
        this(msg + " (Locator=" + locator + " on device)");
    }
}
