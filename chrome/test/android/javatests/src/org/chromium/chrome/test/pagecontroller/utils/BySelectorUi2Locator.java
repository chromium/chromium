// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import androidx.annotation.NonNull;
import androidx.test.uiautomator.BySelector;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject2;

import java.util.List;

/** Locator wrapper around UiAutomator BySelector. */
class BySelectorUi2Locator implements IUi2Locator {
    private final BySelector mSelector;

    public BySelectorUi2Locator(BySelector selector) {
        mSelector = selector;
    }

    @Override
    public UiObject2 locateOne(@NonNull UiDevice device) {
        return device.findObject(mSelector);
    }

    @Override
    public UiObject2 locateOne(@NonNull UiObject2 root) {
        return root.findObject(mSelector);
    }

    @Override
    public List<UiObject2> locateAll(@NonNull UiDevice device) {
        return device.findObjects(mSelector);
    }

    @Override
    public List<UiObject2> locateAll(@NonNull UiObject2 root) {
        return root.findObjects(mSelector);
    }

    @Override
    public String toString() {
        return "BySelectorLocator{" + "mSelector=" + mSelector + '}';
    }
}
