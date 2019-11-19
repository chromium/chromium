// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers;

import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.chrome.test.pagecontroller.utils.UiLocatorHelper;

/**
 * Base class for any controller that needs to perform UI actions and verify UI
 * state.
 *
 * Could represent anything from buttons to a news snippet on the New Tab Page,
 * although simple elements such as buttons can be controlled by
 * helper methods such as {@link UiAutomatorUtils#click} so that implementing
 * ElementControllers for them is usually not necessary.
 *
 * For modal UI such as the 3-dot menu, its controller should subclass
 * PageController.
 */
public class ElementController {
    protected final UiAutomatorUtils mUtils;
    protected final UiLocatorHelper mLocatorHelper;

    public ElementController() {
        mUtils = UiAutomatorUtils.getInstance();
        mLocatorHelper = mUtils.getLocatorHelper();
    }
}
