// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;

import androidx.annotation.NonNull;

import java.util.List;

/**
 * Locator wrapper around UiAutomator BySelector that supports indexing into found nodes.
 */
class BySelectorIndexUi2Locator implements IUi2Locator {
    private final BySelectorUi2Locator mSelectorLocator;
    private final int mIndex;

    public BySelectorIndexUi2Locator(BySelector selector, int index) {
        if (index < 0) {
            throw new IllegalArgumentException("index must be >= 0");
        }
        mSelectorLocator = new BySelectorUi2Locator(selector);
        mIndex = index;
    }

    @Override
    public UiObject2 locateOne(@NonNull UiDevice device) {
        return Utils.nullableGet(mSelectorLocator.locateAll(device), mIndex);
    }

    @Override
    public UiObject2 locateOne(@NonNull UiObject2 root) {
        return Utils.nullableGet(mSelectorLocator.locateAll(root), mIndex);
    }

    @Override
    public List<UiObject2> locateAll(@NonNull UiDevice device) {
        return Utils.nullableIntoList(locateOne(device));
    }

    @Override
    public List<UiObject2> locateAll(@NonNull UiObject2 root) {
        return Utils.nullableIntoList(locateOne(root));
    }

    @Override
    public String toString() {
        return "BySelectorIndexLocator{"
                + "mSelectorLocator=" + mSelectorLocator + ", mIndex=" + mIndex + '}';
    }
}
