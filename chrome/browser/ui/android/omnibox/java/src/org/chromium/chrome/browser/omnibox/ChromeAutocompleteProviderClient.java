// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.metrics.OmniboxEventProtosIntDef.PageClassification;
import org.chromium.components.omnibox.PageClassificationUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Creates the c++ class that provides ChromeAutocompleteProviderClient to access java resources.
 */
@NullMarked
public class ChromeAutocompleteProviderClient {
    @CalledByNative
    // Returns all eligible tabs for the android tab matcher. For most {@link PageClassification}s
    // this is all hidden tabs, but for PageClassification.ANDROID_HUB and
    // PageClassification.ANDROID_TAB_SEARCH_OVERLAY, they include all tabs.
    private static @JniType("std::vector<TabAndroid*>") List<Tab> getAllEligibleTabs(
            TabModel[] tabModels, @PageClassification int pageClassification, int activeTabId) {
        List<Tab> tabs = new ArrayList<>();
        boolean includeActiveTab = PageClassificationUtils.isHubOrTabSearch(pageClassification);
        for (TabModel tabModel : tabModels) {
            if (tabModel == null) continue;

            for (Tab tab : tabModel) {
                if ((includeActiveTab || tab.getId() != activeTabId) && tab.isInitialized()) {
                    tabs.add(tab);
                }
            }
        }
        return tabs;
    }
}
