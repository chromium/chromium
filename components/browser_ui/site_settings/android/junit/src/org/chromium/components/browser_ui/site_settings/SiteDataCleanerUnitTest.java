// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Unit tests for {@link SiteDataCleaner}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SiteDataCleanerUnitTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private WebsitePreferenceBridge.Natives mBridgeMock;

    @Mock
    private BrowserContextHandle mContextHandle;

    private SiteDataCleaner mSiteDataCleaner;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mBridgeMock);
        mSiteDataCleaner = new SiteDataCleaner();
    }

    @Test
    public void testClearData() {
        Website origin1 = new Website(WebsiteAddress.create("https://google.com"), null);
        Website origin2 = new Website(WebsiteAddress.create("mail.google.com"), null);
        Website origin3 = new Website(WebsiteAddress.create("https://docs.google.com"), null);
        WebsiteGroup group = new WebsiteGroup(
                "google.com", new ArrayList<>(Arrays.asList(origin1, origin2, origin3)));
        final AtomicInteger callbacksReceived = new AtomicInteger(0);
        final Runnable callback = () -> {
            callbacksReceived.incrementAndGet();
        };
        mSiteDataCleaner.clearData(mContextHandle, group, callback);
        // Check that the callback was invoked only once.
        Assert.assertEquals(1, callbacksReceived.get());
        // Verify that the bridge is called for each of the websites.
        for (Website website : group.getWebsites()) {
            verify(mBridgeMock).clearCookieData(mContextHandle, website.getAddress().getOrigin());
            verify(mBridgeMock).clearBannerData(mContextHandle, website.getAddress().getOrigin());
            verify(mBridgeMock)
                    .clearMediaLicenses(mContextHandle, website.getAddress().getOrigin());
        }
    }
}
