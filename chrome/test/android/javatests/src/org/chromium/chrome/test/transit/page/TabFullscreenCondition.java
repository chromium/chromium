// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.test.util.FullscreenTestUtils;

import java.util.function.Supplier;

/** Fulfilled when a Tab is in standard fullscreen mode, with the status bar hidden. */
@NullMarked
public class TabFullscreenCondition extends UiThreadCondition {
    private final Supplier<@Nullable Tab> mTabSupplier;

    public TabFullscreenCondition(Supplier<@Nullable Tab> tabSupplier) {
        mTabSupplier = dependOnSupplier(tabSupplier, "Tab");
    }

    @Override
    public String buildDescription() {
        return "Tab is in fullscreen";
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        Tab tab = assumeNonNull(mTabSupplier.get());
        if (!FullscreenTestUtils.isFullscreenInsetsApiMigrationEnabled()) {
            boolean fullscreenFlagSet =
                    FullscreenTestUtils.isFullscreenFlagSet(tab, /* state= */ true);
            return whether(fullscreenFlagSet, "fullscreenFlagSet=%b", fullscreenFlagSet);
        }
        TabWebContentsDelegateAndroid delegate = TabTestUtils.getTabWebContentsDelegate(tab);
        boolean fullscreenForTab = delegate.isFullscreenForTabOrPending();
        boolean statusBarHidden =
                FullscreenTestUtils.verifyWindowInsets(tab, /* expectStatusBarHidden= */ true);
        return whether(
                fullscreenForTab && statusBarHidden,
                "fullscreenForTab=%b, statusBarHidden=%b",
                fullscreenForTab,
                statusBarHidden);
    }
}
