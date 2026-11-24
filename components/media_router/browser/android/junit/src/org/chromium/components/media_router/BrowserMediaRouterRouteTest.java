// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.WebContents;

import java.util.Collections;

/** Route tests for BrowserMediaRouter. */
@RunWith(BaseRobolectricTestRunner.class)
public class BrowserMediaRouterRouteTest extends BrowserMediaRouterTestBase {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock WebContents mWebContents1;

    @Mock WebContents mWebContents2;

    @Override
    public void setUp() {
        super.setUp();
        doReturn(false).when(mWebContents1).isIncognito();
        doReturn(false).when(mWebContents2).isIncognito();

        MediaRouterClient.setInstance(
                new TestMediaRouterClient() {
                    @Override
                    public int getTabId(WebContents webContents) {
                        if (webContents == mWebContents1) return TAB_ID1;
                        return TAB_ID2;
                    }
                });
    }

    @After
    public void tearDown() {
        MediaRouterClient.setInstance(null);
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCreateOneRoute() {
        assertEquals(mBrowserMediaRouter.getRouteIdsToProvidersForTest().size(), 0);

        mBrowserMediaRouter.createRoute(
                SOURCE_ID1, SINK_ID1, PRESENTATION_ID1, ORIGIN1, mWebContents1, REQUEST_ID1);
        verify(mRouteProvider)
                .createRoute(
                        SOURCE_ID1,
                        SINK_ID1,
                        PRESENTATION_ID1,
                        ORIGIN1,
                        TAB_ID1,
                        false,
                        REQUEST_ID1);

        String routeId1 = new MediaRoute(SINK_ID1, SOURCE_ID1, PRESENTATION_ID1).id;
        mBrowserMediaRouter.onRouteCreated(routeId1, SINK_ID1, REQUEST_ID1, mRouteProvider, true);

        assertEquals(1, mBrowserMediaRouter.getRouteIdsToProvidersForTest().size());
        assertTrue(mBrowserMediaRouter.getRouteIdsToProvidersForTest().containsKey(routeId1));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCreateTwoRoutes() {
        mBrowserMediaRouter.createRoute(
                SOURCE_ID1, SINK_ID1, PRESENTATION_ID1, ORIGIN1, mWebContents1, REQUEST_ID1);

        String routeId1 = new MediaRoute(SINK_ID1, SOURCE_ID1, PRESENTATION_ID1).id;
        mBrowserMediaRouter.onRouteCreated(routeId1, SINK_ID1, REQUEST_ID1, mRouteProvider, true);

        mBrowserMediaRouter.createRoute(
                SOURCE_ID2, SINK_ID2, PRESENTATION_ID2, ORIGIN2, mWebContents2, REQUEST_ID2);

        verify(mRouteProvider)
                .createRoute(
                        SOURCE_ID2,
                        SINK_ID2,
                        PRESENTATION_ID2,
                        ORIGIN2,
                        TAB_ID2,
                        false,
                        REQUEST_ID2);
        String routeId2 = new MediaRoute(SINK_ID2, SOURCE_ID2, PRESENTATION_ID2).id;
        mBrowserMediaRouter.onRouteCreated(routeId2, SINK_ID2, REQUEST_ID2, mRouteProvider, true);

        assertEquals(2, mBrowserMediaRouter.getRouteIdsToProvidersForTest().size());
        assertTrue(mBrowserMediaRouter.getRouteIdsToProvidersForTest().containsKey(routeId2));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCreateRouteFails() {
        mBrowserMediaRouter.createRoute(
                SOURCE_ID1, SINK_ID1, PRESENTATION_ID1, ORIGIN1, mWebContents1, REQUEST_ID1);

        verify(mRouteProvider)
                .createRoute(
                        SOURCE_ID1,
                        SINK_ID1,
                        PRESENTATION_ID1,
                        ORIGIN1,
                        TAB_ID1,
                        false,
                        REQUEST_ID1);
        mBrowserMediaRouter.onCreateRouteRequestError("ERROR", REQUEST_ID1);

        assertEquals(0, mBrowserMediaRouter.getRouteIdsToProvidersForTest().size());
    }

    @Test
    @Feature({"MediaRouter"})
    public void testJoinRoute() {
        mBrowserMediaRouter.createRoute(
                SOURCE_ID1, SINK_ID1, PRESENTATION_ID1, ORIGIN1, mWebContents1, REQUEST_ID1);

        String routeId1 = new MediaRoute(SINK_ID1, SOURCE_ID1, PRESENTATION_ID1).id;
        mBrowserMediaRouter.onRouteCreated(routeId1, SINK_ID1, REQUEST_ID1, mRouteProvider, true);

        mBrowserMediaRouter.joinRoute(
                SOURCE_ID2, PRESENTATION_ID1, ORIGIN1, mWebContents2, REQUEST_ID2);
        verify(mRouteProvider)
                .joinRoute(SOURCE_ID2, PRESENTATION_ID1, ORIGIN1, TAB_ID2, REQUEST_ID2);

        String routeId2 = new MediaRoute(SINK_ID1, SOURCE_ID2, PRESENTATION_ID2).id;
        mBrowserMediaRouter.onRouteCreated(routeId2, SINK_ID1, REQUEST_ID2, mRouteProvider, true);

        assertEquals(2, mBrowserMediaRouter.getRouteIdsToProvidersForTest().size());
        assertTrue(mBrowserMediaRouter.getRouteIdsToProvidersForTest().containsKey(routeId2));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testJoinRouteFails() {
        mBrowserMediaRouter.createRoute(
                SOURCE_ID1, SINK_ID1, PRESENTATION_ID1, ORIGIN1, mWebContents1, REQUEST_ID1);

        String routeId1 = new MediaRoute(SINK_ID1, SOURCE_ID1, PRESENTATION_ID1).id;
        mBrowserMediaRouter.onRouteCreated(routeId1, SINK_ID1, REQUEST_ID1, mRouteProvider, true);

        mBrowserMediaRouter.joinRoute(
                SOURCE_ID2, PRESENTATION_ID1, ORIGIN1, mWebContents2, REQUEST_ID2);
        verify(mRouteProvider)
                .joinRoute(SOURCE_ID2, PRESENTATION_ID1, ORIGIN1, TAB_ID2, REQUEST_ID2);

        mBrowserMediaRouter.onJoinRouteRequestError("error", REQUEST_ID2);

        assertEquals(1, mBrowserMediaRouter.getRouteIdsToProvidersForTest().size());
    }

    @Test
    @Feature({"MediaRouter"})
    public void testDetachRoute() {
        mBrowserMediaRouter.createRoute(
                SOURCE_ID1, SINK_ID1, PRESENTATION_ID1, ORIGIN1, mWebContents1, REQUEST_ID1);

        String routeId1 = new MediaRoute(SINK_ID1, SOURCE_ID1, PRESENTATION_ID1).id;
        mBrowserMediaRouter.onRouteCreated(routeId1, SINK_ID1, REQUEST_ID1, mRouteProvider, true);

        mBrowserMediaRouter.detachRoute(routeId1);
        verify(mRouteProvider).detachRoute(routeId1);

        assertEquals(0, mBrowserMediaRouter.getRouteIdsToProvidersForTest().size());
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCloseRoute() {
        mBrowserMediaRouter.createRoute(
                SOURCE_ID1, SINK_ID1, PRESENTATION_ID1, ORIGIN1, mWebContents1, REQUEST_ID1);

        String routeId1 = new MediaRoute(SINK_ID1, SOURCE_ID1, PRESENTATION_ID1).id;
        mBrowserMediaRouter.onRouteCreated(routeId1, SINK_ID1, REQUEST_ID1, mRouteProvider, true);

        mBrowserMediaRouter.closeRoute(routeId1);
        verify(mRouteProvider).closeRoute(routeId1);
        assertEquals(1, mBrowserMediaRouter.getRouteIdsToProvidersForTest().size());

        mBrowserMediaRouter.onRouteTerminated(routeId1);
        assertEquals(0, mBrowserMediaRouter.getRouteIdsToProvidersForTest().size());
    }

    @Test
    @Feature({"MediaRouter"})
    public void testTeardownTearsDownProvidersAndClearsState() {
        MediaRouteProvider secondProvider = mock(MediaRouteProvider.class);
        mBrowserMediaRouter.addMediaRouteProvider(secondProvider);

        mBrowserMediaRouter.createRoute(
                SOURCE_ID1, SINK_ID1, PRESENTATION_ID1, ORIGIN1, mWebContents1, REQUEST_ID1);
        String routeId1 = new MediaRoute(SINK_ID1, SOURCE_ID1, PRESENTATION_ID1).id;
        mBrowserMediaRouter.onRouteCreated(routeId1, SINK_ID1, REQUEST_ID1, mRouteProvider, true);
        mBrowserMediaRouter.onSinksReceived(
                SOURCE_ID1,
                mRouteProvider,
                Collections.singletonList(new MediaSink(SINK_ID1, SINK_NAME1)));

        mBrowserMediaRouter.teardown();

        verify(mRouteProvider).teardown();
        verify(secondProvider).teardown();
        assertTrue(mBrowserMediaRouter.getRouteProvidersForTest().isEmpty());
        assertTrue(mBrowserMediaRouter.getRouteIdsToProvidersForTest().isEmpty());
        assertTrue(mBrowserMediaRouter.getSinksPerSourceForTest().isEmpty());
        assertTrue(mBrowserMediaRouter.getSinksPerSourcePerProviderForTest().isEmpty());

        // Calling teardown again is a safe no-op and torn-down routers no longer receive
        // permission broadcasts.
        mBrowserMediaRouter.teardown();
        BrowserMediaRouter.setNeedsLocalNetworkPermissionForTesting(null);
        BrowserMediaRouter.onLocalNetworkPermissionResult(/* granted= */ true);
        verify(mRouteProvider, times(1)).teardown();
        verify(secondProvider, times(1)).teardown();
        verify(mRouteProvider, never()).onLocalNetworkPermissionGranted();
        verify(secondProvider, never()).onLocalNetworkPermissionGranted();
    }
}
