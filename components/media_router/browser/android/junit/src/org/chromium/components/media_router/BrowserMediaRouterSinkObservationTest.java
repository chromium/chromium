// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.BaseSwitches;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;

import java.util.ArrayList;
import java.util.List;

/** Sink observation tests for BrowserMediaRouter. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BrowserMediaRouterSinkObservationTest extends BrowserMediaRouterTestBase {
    @Test
    @Feature({"MediaRouter"})
    public void testRouterOnSinksReceived() {
        mBrowserMediaRouter.onSinksReceived(SOURCE_ID1, mRouteProvider, new ArrayList<MediaSink>());

        assertEquals(1, mBrowserMediaRouter.getSinksPerSourcePerProviderForTest().size());
        assertEquals(
                1,
                mBrowserMediaRouter.getSinksPerSourcePerProviderForTest().get(SOURCE_ID1).size());
        assertEquals(
                0,
                mBrowserMediaRouter
                        .getSinksPerSourcePerProviderForTest()
                        .get(SOURCE_ID1)
                        .get(mRouteProvider)
                        .size());
        assertEquals(1, mBrowserMediaRouter.getSinksPerSourceForTest().size());
        assertEquals(0, mBrowserMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID1).size());
    }

    @Test
    @Feature({"MediaRouter"})
    public void testRouterOnSinksReceivedTwiceForOneSource() {
        mBrowserMediaRouter.onSinksReceived(SOURCE_ID1, mRouteProvider, new ArrayList<MediaSink>());

        List<MediaSink> sinkList = new ArrayList<MediaSink>();
        MediaSink sink = new MediaSink(SINK_ID1, SINK_NAME1, null);
        sinkList.add(sink);
        mBrowserMediaRouter.onSinksReceived(SOURCE_ID1, mRouteProvider, sinkList);

        assertEquals(1, mBrowserMediaRouter.getSinksPerSourcePerProviderForTest().size());
        assertEquals(
                1,
                mBrowserMediaRouter.getSinksPerSourcePerProviderForTest().get(SOURCE_ID1).size());
        assertEquals(
                1,
                mBrowserMediaRouter
                        .getSinksPerSourcePerProviderForTest()
                        .get(SOURCE_ID1)
                        .get(mRouteProvider)
                        .size());
        assertTrue(
                mBrowserMediaRouter
                        .getSinksPerSourcePerProviderForTest()
                        .get(SOURCE_ID1)
                        .get(mRouteProvider)
                        .contains(sink));

        assertEquals(1, mBrowserMediaRouter.getSinksPerSourceForTest().size());
        assertEquals(1, mBrowserMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID1).size());
        assertTrue(mBrowserMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID1).contains(sink));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testRouterOnSinksReceivedForTwoSources() {
        mBrowserMediaRouter.onSinksReceived(SOURCE_ID1, mRouteProvider, new ArrayList<MediaSink>());

        List<MediaSink> sinkList = new ArrayList<MediaSink>();
        MediaSink sink = new MediaSink(SINK_ID1, SINK_NAME1, null);
        sinkList.add(sink);
        mBrowserMediaRouter.onSinksReceived(SOURCE_ID2, mRouteProvider, sinkList);

        assertEquals(2, mBrowserMediaRouter.getSinksPerSourcePerProviderForTest().size());
        assertEquals(
                1,
                mBrowserMediaRouter.getSinksPerSourcePerProviderForTest().get(SOURCE_ID1).size());
        assertEquals(
                0,
                mBrowserMediaRouter
                        .getSinksPerSourcePerProviderForTest()
                        .get(SOURCE_ID1)
                        .get(mRouteProvider)
                        .size());
        assertEquals(
                1,
                mBrowserMediaRouter.getSinksPerSourcePerProviderForTest().get(SOURCE_ID2).size());
        assertEquals(
                1,
                mBrowserMediaRouter
                        .getSinksPerSourcePerProviderForTest()
                        .get(SOURCE_ID2)
                        .get(mRouteProvider)
                        .size());
        assertTrue(
                mBrowserMediaRouter
                        .getSinksPerSourcePerProviderForTest()
                        .get(SOURCE_ID2)
                        .get(mRouteProvider)
                        .contains(sink));
        assertEquals(2, mBrowserMediaRouter.getSinksPerSourceForTest().size());
        assertEquals(0, mBrowserMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID1).size());
        assertEquals(1, mBrowserMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID2).size());
        assertTrue(mBrowserMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID2).contains(sink));
    }

    @Test
    @Feature({"MediaRouter"})
    @CommandLineFlags.Add({BaseSwitches.DISABLE_LOW_END_DEVICE_MODE})
    public void testNotLowRamDevice() {
        assertTrue(mBrowserMediaRouter.startObservingMediaSinks(SOURCE_ID1));
    }

    @Test
    @Feature({"MediaRouter"})
    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    public void testIsLowRamDevice() {
        assertFalse(mBrowserMediaRouter.startObservingMediaSinks(SOURCE_ID1));
    }
}
