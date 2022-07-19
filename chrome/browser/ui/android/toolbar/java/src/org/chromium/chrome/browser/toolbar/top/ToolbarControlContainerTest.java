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
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarAllowCaptureReason;
import org.chromium.chrome.browser.toolbar.top.CaptureReadinessResult.TopToolbarBlockCaptureReason;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer.ToolbarViewResourceAdapter;
import org.chromium.chrome.browser.toolbar.top.ToolbarSnapshotState.ToolbarSnapshotDifference;

/** Unit tests for ToolbarControlContainer. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {HomeButtonCoordinatorTest.ShadowChromeFeatureList.class})
public class ToolbarControlContainerTest {
    @Rule
    public MockitoRule rule = MockitoJUnit.rule();
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private ResourceFactory.Natives mResourceFactoryJni;

    @Mock
    private View mToolbarContainer;
    @Mock
    private Toolbar mToolbar;

    @Before
    public void before() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(ResourceFactoryJni.TEST_HOOKS, mResourceFactoryJni);
        UmaRecorderHolder.resetForTesting();
        Mockito.when(mToolbarContainer.getWidth()).thenReturn(1);
        Mockito.when(mToolbarContainer.getHeight()).thenReturn(1);
    }

    @Test
    @DisabledTest(message = "Temporarily disabled due to https://crbug.com/1344612")
    public void testIsDirty() {
        ToolbarViewResourceAdapter adapter =
                new ToolbarViewResourceAdapter(mToolbarContainer, false);
        adapter.setOnResourceReadyCallback((resource) -> {});

        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason"));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason"));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.SnapshotDifference"));

        Assert.assertFalse(adapter.isDirty());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.TOOLBAR_OR_RESULT_NULL));

        adapter.setToolbar(mToolbar);
        Assert.assertFalse(adapter.isDirty());
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.TOOLBAR_OR_RESULT_NULL));

        Mockito.when(mToolbar.isReadyForTextureCapture())
                .thenReturn(CaptureReadinessResult.unknown(true));
        Assert.assertTrue(adapter.isDirty());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarBlockCaptureReason.UNKNOWN));

        adapter.triggerBitmapCapture();
        Assert.assertFalse(adapter.isDirty());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.BlockCaptureReason",
                        TopToolbarBlockCaptureReason.VIEW_NOT_DIRTY));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.TopToolbar.SnapshotDifference"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarBlockCaptureReason.UNKNOWN));

        adapter.forceInvalidate();
        Assert.assertTrue(adapter.isDirty());
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarBlockCaptureReason.UNKNOWN));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
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
                RecordHistogram.getHistogramValueCountForTesting(
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
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarAllowCaptureReason.FORCE_CAPTURE));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
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
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.AllowCaptureReason",
                        TopToolbarAllowCaptureReason.SNAPSHOT_DIFFERENCE));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.TopToolbar.SnapshotDifference",
                        ToolbarSnapshotDifference.URL_TEXT));
    }
}
