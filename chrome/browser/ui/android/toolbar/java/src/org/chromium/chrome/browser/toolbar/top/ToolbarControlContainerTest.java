// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarAllowCaptureReason;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer.ToolbarViewResourceAdapter;
import org.chromium.chrome.browser.toolbar.top.ToolbarSnapshotState.ToolbarSnapshotDifference;

/** Unit tests for ToolbarControlContainer. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {HomeButtonCoordinatorTest.ShadowChromeFeatureList.class,
                ShadowRecordHistogram.class})
public class ToolbarControlContainerTest {
    @Rule
    public MockitoRule rule = MockitoJUnit.rule();

    @Mock
    private View mToolbarContainer;
    @Mock
    private Toolbar mToolbar;

    @Before
    public void before() {
        ShadowRecordHistogram.reset();
        Mockito.when(mToolbarContainer.getWidth()).thenReturn(1);
        Mockito.when(mToolbarContainer.getHeight()).thenReturn(1);
    }

    @Test
    public void testIsDirty() {
        ToolbarViewResourceAdapter adapter =
                new ToolbarViewResourceAdapter(mToolbarContainer, false);
        Assert.assertEquals(0,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason"));
        Assert.assertEquals(0,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason"));
        Assert.assertEquals(0,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.SnapshotDifference"));

        Assert.assertFalse(adapter.isDirty());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.TOOLBAR_OR_RESULT_NULL));

        adapter.setToolbar(mToolbar);
        Assert.assertFalse(adapter.isDirty());
        Assert.assertEquals(2,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.TOOLBAR_OR_RESULT_NULL));

        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.unknown(true));
        Assert.assertTrue(adapter.isDirty());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarBlockCaptureReason.UNKNOWN));

        adapter.getBitmap();
        Assert.assertFalse(adapter.isDirty());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.VIEW_NOT_DIRTY));
        Assert.assertEquals(0,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.SnapshotDifference"));

        // Need to be careful here. #getBitmap() in debug builds will call isDirty. Reset histogram
        // tracking to avoid being needing to depend on build type.
        ShadowRecordHistogram.reset();

        Assert.assertEquals(0,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarBlockCaptureReason.UNKNOWN));

        adapter.forceInvalidate();
        Assert.assertTrue(adapter.isDirty());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarBlockCaptureReason.UNKNOWN));
        Assert.assertEquals(0,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.SnapshotDifference"));
    }

    @Test
    public void testIsDirty_BlockedReason() {
        ToolbarViewResourceAdapter adapter =
                new ToolbarViewResourceAdapter(mToolbarContainer, false);
        adapter.setToolbar(mToolbar);
        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.notReady(
                        TopToolbarBlockCaptureReason.SNAPSHOT_SAME));
        Assert.assertFalse(adapter.isDirty());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.SNAPSHOT_SAME));
    }

    @Test
    public void testIsDirty_AllowForced() {
        ToolbarViewResourceAdapter adapter =
                new ToolbarViewResourceAdapter(mToolbarContainer, false);
        adapter.setToolbar(mToolbar);
        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyForced());
        Assert.assertTrue(adapter.isDirty());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarAllowCaptureReason.FORCE_CAPTURE));
        Assert.assertEquals(0,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.SnapshotDifference"));
    }

    @Test
    public void testIsDirty_AllowSnapshotReason() {
        ToolbarViewResourceAdapter adapter =
                new ToolbarViewResourceAdapter(mToolbarContainer, false);
        adapter.setToolbar(mToolbar);
        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.readyWithSnapshotDifference(
                        ToolbarSnapshotDifference.URL_TEXT));
        Assert.assertTrue(adapter.isDirty());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE));
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.SnapshotDifference",
                        ToolbarSnapshotDifference.URL_TEXT));
    }
}