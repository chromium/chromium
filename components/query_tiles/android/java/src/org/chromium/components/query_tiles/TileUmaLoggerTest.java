// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.query_tiles;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests for TileUmaLogger.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TileUmaLoggerTest {
    private TileUmaLogger mTileUmaLogger;
    private TestTileProvider mTileProvider;

    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
        mTileProvider = new TestTileProvider(2, 12);
        mTileUmaLogger = new TileUmaLogger("TestUiSurface");
    }

    @After
    public void tearDown() {}

    @Test
    public void testTileClicked() {
        mTileProvider.getQueryTiles(null, tiles -> {
            mTileUmaLogger.recordTilesLoaded(tiles);

            // Click top level tiles.
            mTileUmaLogger.recordTileClicked(mTileProvider.getTileAt(0));
            mTileUmaLogger.recordTileClicked(mTileProvider.getTileAt(11));
            assertHistogramClicked(0, 1);
            assertHistogramClicked(1, 0);
            assertHistogramClicked(11, 1);
            assertHistogramClicked(100, 0);
            assertHistogramClicked(111, 0);
            assertHistogramClicked(1200, 0);

            // Click next level tiles.
            mTileUmaLogger.recordTileClicked(mTileProvider.getTileAt(2, 3));
            mTileUmaLogger.recordTileClicked(mTileProvider.getTileAt(2, 3));
            assertHistogramClicked(0, 1);
            assertHistogramClicked(303, 2);
            assertHistogramClicked(11, 1);
        });
    }

    private void assertHistogramClicked(int tileHistogramIndex, int expected) {
        int actual = RecordHistogram.getHistogramValueCountForTesting(
                "Search.TestUiSurface.Tile.Clicked", tileHistogramIndex);
        Assert.assertEquals(expected, actual);
    }
}
