// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import androidx.annotation.Nullable;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject2;

import java.util.List;

/** This interface unifies the various ways to find a UI node. */
public interface IUi2Locator {
    /**
     * Locates a single node among all nodes found by the locator.
     *
     * @param device The device to search under.
     * @return The first node found by the locator, or null if none is found.
     */
    @Nullable
    UiObject2 locateOne(UiDevice device);

    /**
     * Locates a single node among all nodes found by the locator.
     *
     * @param root The node to search under.
     * @return The first node found by the locator, or null if none is found.
     */
    @Nullable
    UiObject2 locateOne(UiObject2 root);

    /**
     * Locates all nodes found by the locator.
     *
     * @param device The device to search under.
     * @return All nodes found, or an empty list of none are found.
     */
    List<UiObject2> locateAll(UiDevice device);

    /**
     * Locates all nodes found by the locator.
     *
     * @param root The node to search under.
     * @return All nodes found, or an empty list of none are found.
     */
    List<UiObject2> locateAll(UiObject2 root);
}
