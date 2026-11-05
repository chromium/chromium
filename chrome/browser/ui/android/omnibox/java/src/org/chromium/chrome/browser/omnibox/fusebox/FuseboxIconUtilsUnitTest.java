// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;

import android.content.res.Resources;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.IconProto.Icon;
import org.chromium.components.omnibox.IconResourceIdsProtoIntDef.IconResourceIds;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;

/** Unit tests for {@link FuseboxIconUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxIconUtilsUnitTest {
    @Test
    public void getResIdForIconId_mappedIds() {
        assertEquals(
                R.drawable.ic_globe_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.GLOBE));
        assertEquals(
                R.drawable.search_spark_black_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.SEARCH_LOUPE_WITH_SPARKLE));
        assertEquals(
                R.drawable.image_create_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.IMAGE_CREATE));
        assertEquals(
                R.drawable.create_image_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.BANANA));
        assertEquals(
                R.drawable.travel_explore_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.TRAVEL_EXPLORE));
        assertEquals(
                R.drawable.draft_spark_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.DRAFT_SPARK));
        assertEquals(
                R.drawable.autorenew_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.AUTORENEW));
        assertEquals(
                R.drawable.ic_timer, FuseboxIconUtils.getResIdForIconId(IconResourceIds.TIMER));
        assertEquals(
                R.drawable.bolt_24dp, FuseboxIconUtils.getResIdForIconId(IconResourceIds.BOLT));
        assertEquals(
                R.drawable.task_spark_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.TASK_SPARK));
        assertEquals(
                R.drawable.acute_24dp, FuseboxIconUtils.getResIdForIconId(IconResourceIds.ACUTE));
        assertEquals(
                R.drawable.add_photo_alternate_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.ADD_PHOTO_ALTERNATE));
        assertEquals(
                R.drawable.photo_camera_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.LENS_CAMERA));
        assertEquals(
                R.drawable.gs_drive_vd_theme_24,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.DRIVE));
        assertEquals(
                R.drawable.pro_model_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.PTM_TOOL));
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_FUSEBOX_POPUP_VARIATIONS)
    public void getResIdForIconId_attachFile_variationsBottomSheetCarousel_isFolder() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(true);
        OmniboxFeatures.setUseCarouselForTesting(true);
        OmniboxFeatures.setUseScrollableCarouselForTesting(false);
        assertEquals(
                R.drawable.ic_folder_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.ATTACH_FILE));
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_FUSEBOX_POPUP_VARIATIONS)
    public void getResIdForIconId_attachFile_variationsBottomSheetVertical_isFolder() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(true);
        OmniboxFeatures.setUseCarouselForTesting(false);
        OmniboxFeatures.setUseScrollableCarouselForTesting(false);
        assertEquals(
                R.drawable.ic_folder_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.ATTACH_FILE));
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_FUSEBOX_POPUP_VARIATIONS)
    public void getResIdForIconId_attachFile_variationsDisabledBottomSheetCarousel_isAttachFile() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(true);
        OmniboxFeatures.setUseCarouselForTesting(true);
        OmniboxFeatures.setUseScrollableCarouselForTesting(false);
        assertEquals(
                R.drawable.ic_attach_file_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.ATTACH_FILE));
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_FUSEBOX_POPUP_VARIATIONS)
    public void getResIdForIconId_attachFile_bottomSheetScrollableCarousel_isAttachFile() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(true);
        OmniboxFeatures.setUseCarouselForTesting(true);
        OmniboxFeatures.setUseScrollableCarouselForTesting(true);
        assertEquals(
                R.drawable.ic_attach_file_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.ATTACH_FILE));
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_FUSEBOX_POPUP_VARIATIONS)
    public void getResIdForIconId_attachFile_contextMenu_isAttachFile() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(false);
        OmniboxFeatures.setUseCarouselForTesting(false);
        OmniboxFeatures.setUseScrollableCarouselForTesting(false);
        assertEquals(
                R.drawable.ic_attach_file_24dp,
                FuseboxIconUtils.getResIdForIconId(IconResourceIds.ATTACH_FILE));
    }

    @Test
    public void getResIdForIconId_unmappedIds() {
        // PLACE_WHITE (0) is treated as unspecified.
        assertEquals(
                Resources.ID_NULL, FuseboxIconUtils.getResIdForIconId(IconResourceIds.PLACE_WHITE));
        assertEquals(Resources.ID_NULL, FuseboxIconUtils.getResIdForIconId(IconResourceIds.SOCCER));
        // Values unknown to the client, e.g. newly served by the backend.
        assertEquals(Resources.ID_NULL, FuseboxIconUtils.getResIdForIconId(-1));
        assertEquals(Resources.ID_NULL, FuseboxIconUtils.getResIdForIconId(Integer.MAX_VALUE));
    }

    @Test
    public void resolveIconRes_mapsKnownIconOrFallsBack() {
        assertEquals(
                R.drawable.bolt_24dp,
                FuseboxIconUtils.resolveIconRes(
                        Icon.newBuilder().setIconIdValue(IconResourceIds.BOLT).build(),
                        R.drawable.ic_globe_24dp));
        assertEquals(
                R.drawable.ic_globe_24dp,
                FuseboxIconUtils.resolveIconRes(/* icon= */ null, R.drawable.ic_globe_24dp));
        assertEquals(
                R.drawable.ic_globe_24dp,
                FuseboxIconUtils.resolveIconRes(
                        Icon.getDefaultInstance(), R.drawable.ic_globe_24dp));
        assertEquals(
                R.drawable.ic_globe_24dp,
                FuseboxIconUtils.resolveIconRes(
                        Icon.newBuilder().setIconIdValue(IconResourceIds.SOCCER).build(),
                        R.drawable.ic_globe_24dp));
    }
}
