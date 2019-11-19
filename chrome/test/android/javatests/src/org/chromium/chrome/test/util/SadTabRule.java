// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import org.junit.rules.ExternalResource;

import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Initialize a SadTab instance stubbed for facilitating tests.
 */
public class SadTabRule extends ExternalResource {
    private Tab mTab;
    private SadTab mSadTab;

    @Override
    protected void after() {
        if (mSadTab != null) show(false);
    }

    public void setTab(Tab tab) {
        mTab = tab;
    }

    /**
     * Show or hide the stubbed SadTab. |SadTab.isShowing()| will return the status accordingly.
     * @param show {@code true} to show sad tab on UI.
     */
    public void show(boolean show) {
        assert mTab != null;

        if (mSadTab == null) {
            mSadTab = new SadTab(mTab) {
                private boolean mShowing;

                @Override
                public void show() {
                    mShowing = true;
                }

                @Override
                public void removeIfPresent() {
                    mShowing = false;
                }

                @Override
                public boolean isShowing() {
                    return mShowing;
                }
            };
            TestThreadUtils.runOnUiThreadBlocking(() -> SadTab.initForTesting(mTab, mSadTab));
        }
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            if (show) {
                mSadTab.show();
            } else {
                mSadTab.removeIfPresent();
            }
        });
    }
}
