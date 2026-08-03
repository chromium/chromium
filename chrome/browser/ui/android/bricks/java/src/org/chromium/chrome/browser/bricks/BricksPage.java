// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bricks;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.chrome.modules.on_demand.OnDemandModule;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Native page for displaying Bricks (Compose playground). */
@NullMarked
public class BricksPage extends BasicNativePage {
    private static final String TITLE = "Bricks";
    private final BricksCoordinatorInterface mCoordinator;
    private final String mHostName;

    /**
     * Create a new instance of the Bricks page.
     *
     * @param host A NativePageHost to load URLs.
     * @param url The spec URL for the page.
     */
    public BricksPage(NativePageHost host, String url) {
        super(host);
        mHostName =
                url.contains(UrlConstants.BRICKS_JAVA_HOST)
                        ? UrlConstants.BRICKS_JAVA_HOST
                        : UrlConstants.BRICKS_HOST;
        mCoordinator = OnDemandModule.getImpl().createBricksCoordinator(host.getContext(), url);
        initWithView(mCoordinator.getView());
    }

    public BricksPage(NativePageHost host) {
        this(host, UrlConstants.BRICKS_URL);
    }

    @Override
    public String getTitle() {
        return TITLE;
    }

    @Override
    public String getHost() {
        return mHostName;
    }

    @Override
    public void destroy() {
        mCoordinator.destroy();
        super.destroy();
    }
}
