// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;

import android.content.res.Resources;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.IconResourceIdsProto.IconResourceIds;

/** Unit tests for {@link FuseboxIconUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxIconUtilsUnitTest {
    @Test
    public void getResIdForIconId_mappedIds() {
        assertEquals(
                R.drawable.ic_globe_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.GLOBE_VALUE));
        assertEquals(
                R.drawable.search_spark_black_24dp,
                FuseboxIconUtils.getResIdForIconId(
                        IconResourceIds.SEARCH_LOUPE_WITH_SPARKLE_VALUE));
        assertEquals(
                R.drawable.image_create_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.IMAGE_CREATE_VALUE));
        assertEquals(
                R.drawable.create_image_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.BANANA_VALUE));
        assertEquals(
                R.drawable.travel_explore_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.TRAVEL_EXPLORE_VALUE));
        assertEquals(
                R.drawable.draft_spark_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.DRAFT_SPARK_VALUE));
        assertEquals(
                R.drawable.autorenew_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.AUTORENEW_VALUE));
        assertEquals(
                R.drawable.ic_timer,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.TIMER_VALUE));
        assertEquals(
                R.drawable.bolt_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.BOLT_VALUE));
        assertEquals(
                R.drawable.task_spark_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.TASK_SPARK_VALUE));
        assertEquals(
                R.drawable.acute_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.ACUTE_VALUE));
        assertEquals(
                R.drawable.add_photo_alternate_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.ADD_PHOTO_ALTERNATE_VALUE));
    }

    @Test
    public void getResIdForIconId_unmappedIds() {
        // PLACE_WHITE (0) is treated as unspecified.
        assertEquals(
                Resources.ID_NULL,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.PLACE_WHITE_VALUE));
        assertEquals(
                Resources.ID_NULL,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.SOCCER_VALUE));
        // Values unknown to the client, e.g. newly served by the backend.
        assertEquals(Resources.ID_NULL, FuseboxIconUtils.getResIdForIconId(-1));
        assertEquals(Resources.ID_NULL, FuseboxIconUtils.getResIdForIconId(Integer.MAX_VALUE));
    }
}
