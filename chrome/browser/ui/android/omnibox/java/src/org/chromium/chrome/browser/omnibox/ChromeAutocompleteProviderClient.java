// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;

import java.util.ArrayList;
import java.util.List;

/**
 * Creates the c++ class that provides ChromeAutocompleteProviderClient to access java resources.
 */
public class ChromeAutocompleteProviderClient {
    @CalledByNative
    // Returns all eligible tabs for the android tab matcher. For most {@link PageClassification}s
    //  this is all hidden tabs, but for PageClassification.ANDROID_HUB it includes all tabs.
    private static Tab[] getAllEligibleTabs(TabModel[] tabModels, int pageClassification) {
        if (tabModels == null) return null;
        List<Tab> tabList = new ArrayList<>();
        for (TabModel tabModel : tabModels) {
            if (tabModel == null) continue;

            for (int i = 0; i < tabModel.getCount(); i++) {
                Tab tab = tabModel.getTabAt(i);
                if (tab.isHidden() || pageClassification == PageClassification.ANDROID_HUB_VALUE) {
                    tabList.add(tab);
                }
            }
        }
        return tabList.isEmpty() ? null : tabList.toArray(new Tab[0]);
    }
}
