// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.page_insights.PageInsightsMediator.PageInsightsEvent;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsActionsHandler;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.HashMap;

/** Implementation of {@link PageInsightsActionsHandler}. */
class PageInsightsActionHandlerImpl implements PageInsightsActionsHandler {
    private final Supplier<Tab> mTabSupplier;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;
    private final ChildPageNavigator mChildPageNavigator;
    private final PageInsightsEventLogger mPageInsightsEventLogger;

    static interface ChildPageNavigator {
        void navigateToChildPage(int pageId);
    }

    /** Creates and returns a map containing the given {@link PageInsightsActionsHandlerImpl}. */
    static HashMap<String, Object> createContextValues(
            PageInsightsActionHandlerImpl pageInsightsActionHandlerImpl) {
        HashMap<String, Object> contextValues = new HashMap<>();
        contextValues.put(PageInsightsActionsHandler.KEY, pageInsightsActionHandlerImpl);
        return contextValues;
    }

    PageInsightsActionHandlerImpl(
            Supplier<Tab> tabSupplier,
            Supplier<ShareDelegate> shareDelegateSupplier,
            ChildPageNavigator childPageNavigator,
            PageInsightsEventLogger pageInsightsEventLogger) {
        mTabSupplier = tabSupplier;
        mShareDelegateSupplier = shareDelegateSupplier;
        mChildPageNavigator = childPageNavigator;
        mPageInsightsEventLogger = pageInsightsEventLogger;
    }

    @Override
    public void openUrl(String url, boolean doesRequestSpecifySameSession) {
        // TODO(b/286003870): Consider opening a new CCT if doesRequestSpecifySameSession is false.
        mPageInsightsEventLogger.log(PageInsightsEvent.TAP_XSURFACE_VIEW_URL);
        mTabSupplier.get().loadUrl(new LoadUrlParams(url));
    }

    @Override
    public void share(String url, String title) {
        mPageInsightsEventLogger.log(PageInsightsEvent.TAP_XSURFACE_VIEW_SHARE);
        mShareDelegateSupplier
                .get()
                .share(
                        new ShareParams.Builder(mTabSupplier.get().getWindowAndroid(), title, url)
                                .build(),
                        new ChromeShareExtras.Builder().build(),
                        ShareOrigin.PAGE_INSIGHTS);
    }

    @Override
    public void navigateToPageInsightsPage(int pageId) {
        mPageInsightsEventLogger.log(PageInsightsEvent.TAP_XSURFACE_VIEW_CHILD_PAGE);
        mChildPageNavigator.navigateToChildPage(pageId);
    }

    interface PageInsightsEventLogger {

        void log(@PageInsightsEvent int event);
    }
}
