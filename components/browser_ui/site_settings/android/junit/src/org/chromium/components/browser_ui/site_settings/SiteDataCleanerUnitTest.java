// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browsing_data.content.BrowsingDataInfo;
import org.chromium.components.browsing_data.content.BrowsingDataModel;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link SiteDataCleaner}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SiteDataCleanerUnitTest {

    public static final Website ORIGIN_1 =
            new Website(WebsiteAddress.create("https://google.com"), null);
    public static final Website ORIGIN_2 =
            new Website(WebsiteAddress.create("mail.google.com"), null);
    public static final Website ORIGIN_3 =
            new Website(WebsiteAddress.create("https://docs.google.com"), null);
    public static final String GOOGLE_COM = "google.com";
    public static final WebsiteGroup GROUP =
            new WebsiteGroup(
                    GOOGLE_COM, new ArrayList<>(Arrays.asList(ORIGIN_1, ORIGIN_2, ORIGIN_3)));
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private WebsitePreferenceBridge.Natives mBridgeMock;

    @Mock private BrowserContextHandle mContextHandle;

    @Mock private SiteSettingsDelegate mSiteSettingsDelegate;

    @Mock private BrowsingDataModel mBrowsingDataModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mBridgeMock);
        doReturn(mContextHandle).when(mSiteSettingsDelegate).getBrowserContextHandle();
    }

    @Test
    public void testClearData() {
        final AtomicInteger callbacksReceived = new AtomicInteger(0);
        final Runnable callback = callbacksReceived::incrementAndGet;
        SiteDataCleaner.clearData(mSiteSettingsDelegate, GROUP, callback);
        // Check that the callback was invoked only once.
        Assert.assertEquals(1, callbacksReceived.get());
        // Verify that the bridge is called for each of the websites.
        for (Website website : GROUP.getWebsites()) {
            verify(mBridgeMock).clearCookieData(mContextHandle, website.getAddress().getOrigin());
            verify(mBridgeMock).clearBannerData(mContextHandle, website.getAddress().getOrigin());
            verify(mBridgeMock)
                    .clearMediaLicenses(mContextHandle, website.getAddress().getOrigin());
        }
    }

    @Test
    public void testClearDataWithBDM() {
        HashMap<Origin, BrowsingDataInfo> map = buildBrowsingDataModelInfo();

        doReturn(true).when(mSiteSettingsDelegate).isBrowsingDataModelFeatureEnabled();
        doReturn(map).when(mBrowsingDataModel).getBrowsingDataInfo(any(), anyBoolean());

        doAnswer(this::mockBDMCallback)
                .when(mSiteSettingsDelegate)
                .getBrowsingDataModel(any(Callback.class));

        doAnswer(this::mockBDMRemoveCallback)
                .when(mBrowsingDataModel)
                .removeBrowsingData(anyString(), any(Runnable.class));

        final AtomicInteger callbacksReceived = new AtomicInteger(0);
        final Runnable callback = callbacksReceived::incrementAndGet;
        SiteDataCleaner.clearData(mSiteSettingsDelegate, ORIGIN_1, callback);

        // Check that the callback was invoked only once.
        Assert.assertEquals(1, callbacksReceived.get());

        verify(mBridgeMock, times(1))
                .clearBannerData(mContextHandle, ORIGIN_1.getAddress().getOrigin());
        verify(mBridgeMock, times(1))
                .clearCookieData(mContextHandle, ORIGIN_1.getAddress().getOrigin());

        // Media licenses are cleared in the BDM.
        verify(mBridgeMock, times(0))
                .clearMediaLicenses(mContextHandle, ORIGIN_1.getAddress().getOrigin());
    }

    private static HashMap<Origin, BrowsingDataInfo> buildBrowsingDataModelInfo() {
        var map = new HashMap<Origin, BrowsingDataInfo>();
        var origin = Origin.create(new GURL(ORIGIN_1.getAddress().getOrigin()));
        map.put(origin, new BrowsingDataInfo(origin, 0, 100, false));
        return map;
    }

    private Object mockBDMCallback(InvocationOnMock invocation) {
        var callback = (Callback<BrowsingDataModel>) invocation.getArguments()[0];
        callback.onResult(mBrowsingDataModel);
        return null;
    }

    private Object mockBDMRemoveCallback(InvocationOnMock invocation) {
        var callback = (Runnable) invocation.getArguments()[1];
        callback.run();
        return null;
    }
}
