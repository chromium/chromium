// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Creates the c++ class that provides ChromeAutocompleteProviderClient to access java resources.
 */
public class ChromeAutocompleteProviderClient {
    @CalledByNative
    private static Tab[] getAllHiddenTabs(TabModel[] tabModels) {
        if (tabModels == null) return null;
        List<Tab> tabList = new ArrayList<>();

        for (TabModel tabModel : tabModels) {
            if (tabModel == null) continue;

            for (int i = 0; i < tabModel.getCount(); ++i) {
                Tab tab = tabModel.getTabAt(i);
                if (tab.isHidden()) tabList.add(tab);
            }
        }
        return tabList.isEmpty() ? null : tabList.toArray(new Tab[0]);
    }
}
