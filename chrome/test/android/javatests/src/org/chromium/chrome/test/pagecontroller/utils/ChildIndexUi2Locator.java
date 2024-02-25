// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import androidx.annotation.NonNull;
import androidx.test.uiautomator.By;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject2;

import java.util.Arrays;
import java.util.List;

/** Locates a child node based on its position relative to its siblings. */
class ChildIndexUi2Locator implements IUi2Locator {
    private final int mFirstChildIndex;
    private final int[] mDescendantIndices;

    /**
     * Locates the nth child, recursively if more indices are specified.
     *
     * @param childIndex        The index of the child.
     * @param descendantIndices Optional additional indices of descendants.
     */
    public ChildIndexUi2Locator(int childIndex, int... descendantIndices) {
        mFirstChildIndex = childIndex;
        mDescendantIndices = descendantIndices;
    }

    @Override
    public UiObject2 locateOne(@NonNull UiDevice device) {
        List<UiObject2> children = device.findObjects(By.depth(0));
        UiObject2 child = Utils.nullableGet(children, mFirstChildIndex);
        return child == null ? null : locateDescendant(child);
    }

    @Override
    public UiObject2 locateOne(@NonNull UiObject2 root) {
        List<UiObject2> children = root.getChildren();
        UiObject2 child = Utils.nullableGet(children, mFirstChildIndex);
        return child == null ? null : locateDescendant(child);
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
        return "ChildIndex{"
                + "mFirstChildIndex="
                + mFirstChildIndex
                + ", mDescendantIndices="
                + Arrays.toString(mDescendantIndices)
                + '}';
    }

    // Go through list of descendants to find the last child.
    private UiObject2 locateDescendant(@NonNull UiObject2 child) {
        List<UiObject2> children;
        for (int index : mDescendantIndices) {
            children = child.getChildren();
            if (children == null || children.size() <= index) return null;
            child = children.get(index);
        }
        return child;
    }
}
