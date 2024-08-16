// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.junit.Assert.assertEquals;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit tests for {@link AdaptiveToolbarFeatures}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AdaptiveToolbarFeaturesTest {
    private Activity mActivity;
    private TestValues mTestValues = new TestValues();

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
        FeatureList.setTestValues(mTestValues);
    }

    @After
    public void tearDown() {
        FeatureList.setTestValues(null);
    }

    @Test
    @Config(qualifiers = "w390dp-h820dp")
    public void testGetTopSegmentationResultOnPhone() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.NEW_TAB, AdaptiveToolbarButtonVariant.SHARE),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.NEW_TAB);
    }

    @Test
    @Config(qualifiers = "w690dp-h820dp")
    public void testGetTopSegmentationResultWithNTBOnTablet() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.NEW_TAB, AdaptiveToolbarButtonVariant.SHARE),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.SHARE);
    }

    @Test
    @Config(qualifiers = "w690dp-h820dp")
    public void testGetTopSegmentationResultWithBookmarkOnTablet() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS,
                        AdaptiveToolbarButtonVariant.SHARE),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.SHARE);
    }

    @Test
    @Config(qualifiers = "w690dp-h820dp")
    public void testGetTopSegmentationResultWithNoNTBOrBookmarkOnTablet() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.TRANSLATE, AdaptiveToolbarButtonVariant.SHARE),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.TRANSLATE);
    }

    @Test
    @Config(qualifiers = "w690dp-h820dp")
    public void testGetTopSegmentationResultWithOnlyNTBAndBookmarkOnTablet() {
        assertTopResult(
                /* segmentationResults= */ List.of(
                        AdaptiveToolbarButtonVariant.ADD_TO_BOOKMARKS,
                        AdaptiveToolbarButtonVariant.NEW_TAB),
                /* expectedTopResult= */ AdaptiveToolbarButtonVariant.UNKNOWN);
    }

    private void assertTopResult(
            List<Integer> segmentationResults,
            @AdaptiveToolbarButtonVariant int expectedTopResult) {
        assertEquals(
                "Top segmentation result is not as expected.",
                expectedTopResult,
                AdaptiveToolbarFeatures.getTopSegmentationResult(mActivity, segmentationResults));
    }
}
