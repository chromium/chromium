// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;

import java.util.function.Supplier;

/**
 * Fulfilled when a web page has hit test data, so that input can be routed to it.
 *
 * <p>Also fulfilled when the page is hidden or when no hit test data is expected (e.g. native
 * pages, about:blank).
 */
public class PageHitTestDataCondition extends UiThreadCondition {
    private final Supplier<Tab> mLoadedTabSupplier;

    public PageHitTestDataCondition(Supplier<Tab> loadedTabSupplier) {
        mLoadedTabSupplier = dependOnSupplier(loadedTabSupplier, "LoadedTab");
    }

    @Override
    public String buildDescription() {
        return "Page has hit test data";
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        Tab tab = mLoadedTabSupplier.get();
        if (tab.isHidden()) return fulfilled("hidden");

        // There are many cases where we don't expect hit test data to be present for the
        // WebContents.
        if (tab.getUrl().isEmpty()
                || tab.getUrl().getSpec().equals(ContentUrlConstants.ABOUT_BLANK_URL)) {
            return fulfilled("empty or about:blank url");
        }
        if (tab.isNativePage()) return fulfilled("native page");
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return fulfilled("no WebContents");
        NavigationController controller = webContents.getNavigationController();
        // HTTP 204/205 No Content/Download pages will not get committed and won't have hit test
        // data. I don't know why http status codes likes 204/205 aren't propagated to the
        // NavigationEntry and I'm scared to ask. I also don't know why we update the committed
        // index even through the page does not commit. However, since the page hasn't *actually*
        // committed, the lastCommittedUrl is blank and the NavigationController still thinks it's
        // on the initial navigation (which is supposed to become false when there's a committed
        // entry). WebContents#hasUncommittedNavigationInPrimaryMainFrame also for some reason
        // returns false...
        if (controller.isInitialNavigation() && controller.getLastCommittedEntryIndex() != -1) {
            return fulfilled("uncommitted initial navigation");
        }
        boolean hasHitTestData = webContents.getMainFrame().hasHitTestDataForTesting();
        return whether(hasHitTestData, "hasHitTestData=%b", hasHitTestData);
    }
}
