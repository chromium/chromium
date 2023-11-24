// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import androidx.annotation.NonNull;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject2;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Locate node(s) satisfying the chain of IUi2Locators, rooting each iteration of the search at the
 * node(s) found by the previous locator in the chain.
 */
class PathUi2Locator implements IUi2Locator {
    private final IUi2Locator mFirstLocator;
    private final IUi2Locator[] mAdditionalLocators;

    /**
     * Locates the node(s) matching the chain of locators.
     *
     * @param locator            First locator in the chain.
     * @param additionalLocators Optional, additional locators in the chain.
     */
    public PathUi2Locator(@NonNull IUi2Locator firstLocator, IUi2Locator... additionalLocators) {
        mFirstLocator = firstLocator;
        mAdditionalLocators = additionalLocators;
    }

    @Override
    public UiObject2 locateOne(UiDevice device) {
        List<UiObject2> candidates = mFirstLocator.locateAll(device);
        return Utils.nullableGet(locateRestOfPath(candidates), 0);
    }

    @Override
    public UiObject2 locateOne(UiObject2 root) {
        List<UiObject2> candidates = mFirstLocator.locateAll(root);
        return Utils.nullableGet(locateRestOfPath(candidates), 0);
    }

    @Override
    public List<UiObject2> locateAll(UiDevice device) {
        List<UiObject2> candidates = mFirstLocator.locateAll(device);
        return locateRestOfPath(candidates);
    }

    @Override
    public List<UiObject2> locateAll(UiObject2 root) {
        List<UiObject2> candidates = mFirstLocator.locateAll(root);
        return locateRestOfPath(candidates);
    }

    @Override
    public String toString() {
        return "Path{"
                + "mFirstLocator="
                + mFirstLocator
                + ", mAdditionalLocators="
                + Arrays.toString(mAdditionalLocators)
                + '}';
    }

    /**
     * Iterate through mAdditionalLocators, feeding results from each round to the next iteration.
     *
     * @param  initialCandidates Input used as a starting point of the iteration.
     * @return List of UiObject2 located by the last locator in mAdditionalLocators.
     */
    private List<UiObject2> locateRestOfPath(@NonNull List<UiObject2> initialCandidates) {
        List<UiObject2> currentObjects = new ArrayList<>();
        currentObjects.addAll(initialCandidates);

        for (int i = 0; i < mAdditionalLocators.length; i++) {
            List<UiObject2> nextObjects = new ArrayList<>();
            for (UiObject2 currentObject : currentObjects) {
                nextObjects.addAll(mAdditionalLocators[i].locateAll(currentObject));
            }
            currentObjects.clear();
            currentObjects.addAll(nextObjects);
        }

        return currentObjects;
    }
}
