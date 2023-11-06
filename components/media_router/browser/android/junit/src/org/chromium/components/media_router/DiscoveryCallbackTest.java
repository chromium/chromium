// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.mediarouter.media.MediaRouteSelector;
import androidx.mediarouter.media.MediaRouter;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.ArrayList;
import java.util.List;

/** Robolectric tests for DiscoveryCallback. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DiscoveryCallbackTest extends BrowserMediaRouterTestBase {
    protected DiscoveryDelegate mDiscoveryDelegate;

    @Override
    @Before
    public void setUp() {
        super.setUp();
        mDiscoveryDelegate = mock(DiscoveryDelegate.class);
        assertNotNull(mDiscoveryDelegate);
    }

    @Test
    @Feature({"MediaRouter"})
    public void testInitCallbackWithEmptyKnownSinks() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback =
                new DiscoveryCallback(SOURCE_ID1, knownSinks, mDiscoveryDelegate, null);

        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID1), eq(knownSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testInitCallbackWithNonemptyKnownSinks() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        MediaSink sink = new MediaSink(SINK_ID1, SINK_NAME1, null);
        DiscoveryCallback callback =
                new DiscoveryCallback(SOURCE_ID1, knownSinks, mDiscoveryDelegate, null);

        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID1), eq(knownSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackAddOneSink() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback =
                new DiscoveryCallback(SOURCE_ID1, knownSinks, mDiscoveryDelegate, null);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        expectedSinks.add(new MediaSink(SINK_ID1, SINK_NAME1, null));
        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackAddTwoSinks() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback =
                new DiscoveryCallback(SOURCE_ID1, knownSinks, mDiscoveryDelegate, null);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID2, SINK_NAME2));
        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        expectedSinks.add(new MediaSink(SINK_ID1, SINK_NAME1, null));
        expectedSinks.add(new MediaSink(SINK_ID2, SINK_NAME2, null));
        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackAddDuplicateSink() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback =
                new DiscoveryCallback(SOURCE_ID1, knownSinks, mDiscoveryDelegate, null);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        expectedSinks.add(new MediaSink(SINK_ID1, SINK_NAME1, null));
        // Only expect one time. The duplicate add will not be notified.
        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackRemoveSink() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback =
                new DiscoveryCallback(SOURCE_ID1, knownSinks, mDiscoveryDelegate, null);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        callback.onRouteRemoved(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));

        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        // One time for init, one time for remove.
        verify(mDiscoveryDelegate, times(2)).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackRemoveNonexistingSink() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback =
                new DiscoveryCallback(SOURCE_ID1, knownSinks, mDiscoveryDelegate, null);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        callback.onRouteRemoved(null, createMockRouteInfo(SINK_ID2, SINK_NAME2));

        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        // Only one time for init.
        verify(mDiscoveryDelegate, times(1)).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackChangeRouteAddsOneSink() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback =
                new DiscoveryCallback(SOURCE_ID1, knownSinks, mDiscoveryDelegate, null);

        callback.onRouteChanged(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        expectedSinks.add(new MediaSink(SINK_ID1, SINK_NAME1, null));
        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackRemoveSinkAfterRouteChanged() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback =
                new DiscoveryCallback(SOURCE_ID1, knownSinks, mDiscoveryDelegate, null);

        MediaRouter.RouteInfo info = createMockRouteInfo(SINK_ID1, SINK_NAME1);
        callback.onRouteChanged(null, info);

        doReturn(false).when(info).matchesSelector((MediaRouteSelector) isNull());
        callback.onRouteChanged(null, info);

        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        // One time for init, one time for remove.
        verify(mDiscoveryDelegate, times(2)).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackAddSourceUrn() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback =
                new DiscoveryCallback(SOURCE_ID1, knownSinks, mDiscoveryDelegate, null);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        callback.addSourceUrn(SOURCE_ID2);

        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        expectedSinks.add(new MediaSink(SINK_ID1, SINK_NAME1, null));
        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID2), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackAddDuplicateSourceUrn() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback =
                new DiscoveryCallback(SOURCE_ID1, knownSinks, mDiscoveryDelegate, null);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        callback.addSourceUrn(SOURCE_ID1);

        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        expectedSinks.add(new MediaSink(SINK_ID1, SINK_NAME1, null));
        // Only the one time after onRouteAdded().
        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackRemoveSourceUrn() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback =
                new DiscoveryCallback(SOURCE_ID1, knownSinks, mDiscoveryDelegate, null);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        assertTrue(callback.containsSourceUrn(SOURCE_ID1));
        callback.removeSourceUrn(SOURCE_ID1);
        assertFalse(callback.containsSourceUrn(SOURCE_ID1));

        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        // Only the one time for init.
        verify(mDiscoveryDelegate).onSinksReceived(eq(SOURCE_ID1), eq(expectedSinks));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCallbackRemoveNonexistingSourceUrn() {
        List<MediaSink> knownSinks = new ArrayList<MediaSink>();
        DiscoveryCallback callback =
                new DiscoveryCallback(SOURCE_ID1, knownSinks, mDiscoveryDelegate, null);

        callback.onRouteAdded(null, createMockRouteInfo(SINK_ID1, SINK_NAME1));
        callback.removeSourceUrn(SOURCE_ID2);

        List<MediaSink> expectedSinks = new ArrayList<MediaSink>();
        // Only the one time for init.
        verify(mDiscoveryDelegate, never()).onSinksReceived(eq(SOURCE_ID2), eq(expectedSinks));
    }

    private MediaRouter.RouteInfo createMockRouteInfo(String sinkId, String sinkName) {
        MediaRouter.RouteInfo route = spy(TestUtils.createMockRouteInfo(sinkId, sinkName));
        doReturn(true).when(route).matchesSelector((MediaRouteSelector) isNull());
        return route;
    }
}
