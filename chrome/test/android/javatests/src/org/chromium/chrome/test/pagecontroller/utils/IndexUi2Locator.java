// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;

import androidx.annotation.NonNull;

import java.util.List;

/**
 * Locate the ith node in the nodes found by an IUi2Locator.
 */
class IndexUi2Locator implements IUi2Locator {
    private final IUi2Locator mLocator;
    private final int mIndex;

    /**
     * Locates the ith node(s) matching the locator.
     *
     * @param index   Value of i.
     * @param locator First locator in the chain.
     */
    public IndexUi2Locator(int index, @NonNull IUi2Locator locator) {
        mIndex = index;
        mLocator = locator;
    }

    @Override
    public UiObject2 locateOne(UiDevice device) {
        List<UiObject2> candidates = mLocator.locateAll(device);
        return Utils.nullableGet(candidates, mIndex);
    }

    @Override
    public UiObject2 locateOne(UiObject2 root) {
        List<UiObject2> candidates = mLocator.locateAll(root);
        return Utils.nullableGet(candidates, mIndex);
    }

    @Override
    public List<UiObject2> locateAll(UiDevice device) {
        return Utils.nullableIntoList(locateOne(device));
    }

    @Override
    public List<UiObject2> locateAll(UiObject2 root) {
        return Utils.nullableIntoList(locateOne(root));
    }

    @Override
    public String toString() {
        return "IndexUi2Locator{"
                + "mLocator=" + mLocator + ", index=" + mIndex + "}";
    }
}
