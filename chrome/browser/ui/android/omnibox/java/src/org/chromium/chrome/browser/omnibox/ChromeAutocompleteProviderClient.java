// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabNativeUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.omnibox.PageClassificationUtils;

import java.util.Arrays;

/**
 * Creates the c++ class that provides ChromeAutocompleteProviderClient to access java resources.
 */
@NullMarked
public class ChromeAutocompleteProviderClient {
    @CalledByNative
    // Returns all eligible tabs for the android tab matcher. For most {@link PageClassification}s
    // this is all hidden tabs, but for PageClassification.ANDROID_HUB and
    // PageClassification.ANDROID_TAB_SEARCH_OVERLAY, they include all tabs.
    private static @JniType("std::vector<int64_t>") long[] getAllEligibleTabs(
            TabModel[] tabModels, int pageClassification) {
        int totalTabs = 0;
        for (TabModel tabModel : tabModels) {
            if (tabModel == null) continue;

            totalTabs += tabModel.getCount();
        }

        long[] tempTabPtrArray = new long[totalTabs];
        int addedCount = 0;
        for (TabModel tabModel : tabModels) {
            if (tabModel == null) continue;

            for (Tab tab : tabModel) {
                if (tab.isHidden()
                        || PageClassificationUtils.isHubOrTabSearch(pageClassification)) {
                    long nativePtr = TabNativeUtils.getNativePtr(tab);
                    if (nativePtr != 0) {
                        tempTabPtrArray[addedCount++] = nativePtr;
                    }
                }
            }
        }
        return Arrays.copyOf(tempTabPtrArray, addedCount);
    }
}
