// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink_public.common.ContextMenuDataMediaFlags;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.ui.mojom.MenuSourceType;
import org.chromium.url.GURL;

/** Unit tests for {@link ContextMenuParams}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextMenuParamsUnitTest {
    private ContextMenuParams createContextMenuParamsWithMediaFlags(int mediaFlags) {
        return new ContextMenuParams(
                /* nativePtr= */ 0,
                /* menuModelBridge= */ null,
                ContextMenuDataMediaType.VIDEO,
                mediaFlags,
                /* pageUrl= */ GURL.emptyGURL(),
                /* linkUrl= */ GURL.emptyGURL(),
                /* linkText= */ "",
                /* unfilteredLinkUrl= */ GURL.emptyGURL(),
                /* srcUrl= */ GURL.emptyGURL(),
                /* titleText= */ "",
                /* referrer= */ null,
                /* canSaveMedia= */ false,
                /* triggeringTouchXDp= */ 0,
                /* triggeringTouchYDp= */ 0,
                MenuSourceType.TOUCH,
                /* openedFromHighlight= */ false,
                /* openedFromInterestFor= */ false,
                /* interestForNodeID= */ 0,
                /* additionalNavigationParams= */ null);
    }

    @Test
    public void testMediaFlags_CanEnterPip() {
        int mediaFlags = ContextMenuDataMediaFlags.MEDIA_CAN_PICTURE_IN_PICTURE;
        ContextMenuParams params = createContextMenuParamsWithMediaFlags(mediaFlags);

        assertTrue("canPictureInPicture should be true", params.canPictureInPicture());
        assertFalse("isPictureInPicture should be false", params.isPictureInPicture());
    }

    @Test
    public void testMediaFlags_IsInPip() {
        int mediaFlags =
                ContextMenuDataMediaFlags.MEDIA_CAN_PICTURE_IN_PICTURE
                        | ContextMenuDataMediaFlags.MEDIA_PICTURE_IN_PICTURE;
        ContextMenuParams params = createContextMenuParamsWithMediaFlags(mediaFlags);

        assertTrue("canPictureInPicture should be true", params.canPictureInPicture());
        assertTrue("isPictureInPicture should be true", params.isPictureInPicture());
    }

    @Test
    public void testMediaFlags_NoPip() {
        int mediaFlags = ContextMenuDataMediaFlags.MEDIA_NONE;
        ContextMenuParams params = createContextMenuParamsWithMediaFlags(mediaFlags);

        assertFalse("canPictureInPicture should be false", params.canPictureInPicture());
        assertFalse("isPictureInPicture should be false", params.isPictureInPicture());
    }
}
