// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;

import org.junit.Before;
import org.robolectric.shadows.ShadowLog;

/** Robolectric test base class for BrowserMediaRouter. */
public class BrowserMediaRouterTestBase {
    protected static final String SOURCE_ID1 =
            "cast:CCCCCCCC?"
                    + "clientId=11111111111111111&"
                    + "autoJoinPolicy=origin_scoped&"
                    + "launchTimeout=10000";
    protected static final String SOURCE_ID2 =
            "cast:CCCCCCCC?"
                    + "clientId=222222222222222222&"
                    + "autoJoinPolicy=origin_scoped&"
                    + "castLaunchTimeout=10000";
    protected static final String SINK_ID1 =
            "com.google.android.gms/"
                    + ".cast.media.MediaRouteProviderService:cccccccccccccccccccccccccccccccc";
    protected static final String SINK_ID2 =
            "com.google.android.gms/"
                    + ".cast.media.MediaRouteProviderService:dddddddddddddddddddddddddddddddd";
    protected static final String SINK_NAME1 = "sink name 1";
    protected static final String SINK_NAME2 = "sink name 2";
    protected static final String PRESENTATION_ID1 = "mr_CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC";
    protected static final String PRESENTATION_ID2 = "mr_DDDDDDDD-DDDD-DDDD-DDDD-DDDDDDDDDDDD";
    protected static final String ORIGIN1 = "http://www.example1.com/";
    protected static final String ORIGIN2 = "http://www.example2.com/";
    protected static final int TAB_ID1 = 1;
    protected static final int TAB_ID2 = 2;
    protected static final int REQUEST_ID1 = 1;
    protected static final int REQUEST_ID2 = 2;
    protected BrowserMediaRouter mBrowserMediaRouter;
    protected MediaRouteProvider mRouteProvider;

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        mBrowserMediaRouter = spy(new BrowserMediaRouter(0));
        mRouteProvider = mock(MediaRouteProvider.class);
        doReturn(true).when(mRouteProvider).supportsSource(anyString());
        mBrowserMediaRouter.addMediaRouteProvider(mRouteProvider);
        assertEquals(1, mBrowserMediaRouter.getRouteProvidersForTest().size());
        assertEquals(mRouteProvider, mBrowserMediaRouter.getRouteProvidersForTest().get(0));
        assertNotNull(mRouteProvider);
    }
}
