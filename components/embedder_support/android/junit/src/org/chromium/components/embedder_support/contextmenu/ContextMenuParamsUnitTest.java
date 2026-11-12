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
                /* nativePtr= */ null,
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

    @Test
    public void testMediaFlags_EncryptedAndReadableVideoFrame() {
        ContextMenuParams noneParams =
                createContextMenuParamsWithMediaFlags(ContextMenuDataMediaFlags.MEDIA_NONE);
        assertFalse("isEncrypted should be false", noneParams.isEncrypted());
        assertFalse("hasReadableVideoFrame should be false", noneParams.hasReadableVideoFrame());

        ContextMenuParams readableParams =
                createContextMenuParamsWithMediaFlags(
                        ContextMenuDataMediaFlags.MEDIA_HAS_READABLE_VIDEO_FRAME);
        assertFalse("isEncrypted should be false", readableParams.isEncrypted());
        assertTrue("hasReadableVideoFrame should be true", readableParams.hasReadableVideoFrame());

        ContextMenuParams encryptedParams =
                createContextMenuParamsWithMediaFlags(
                        ContextMenuDataMediaFlags.MEDIA_ENCRYPTED
                                | ContextMenuDataMediaFlags.MEDIA_HAS_READABLE_VIDEO_FRAME);
        assertTrue("isEncrypted should be true", encryptedParams.isEncrypted());
        assertTrue("hasReadableVideoFrame should be true", encryptedParams.hasReadableVideoFrame());
    }

    private ContextMenuParams createContextMenuParamsWithMediaType(
            @ContextMenuDataMediaType int mediaType) {
        return new ContextMenuParams(
                /* nativePtr= */ null,
                /* menuModelBridge= */ null,
                mediaType,
                ContextMenuDataMediaFlags.MEDIA_NONE,
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
                MenuSourceType.MOUSE,
                /* openedFromHighlight= */ false,
                /* openedFromInterestFor= */ false,
                /* additionalNavigationParams= */ null);
    }

    @Test
    public void testIsCanvas() {
        ContextMenuParams canvasParams =
                createContextMenuParamsWithMediaType(ContextMenuDataMediaType.CANVAS);
        assertTrue("isCanvas should be true for CANVAS", canvasParams.isCanvas());
        assertFalse("isPage should be false for CANVAS", canvasParams.isPage());
        assertFalse("isImage should be false for CANVAS", canvasParams.isImage());
        assertFalse("isVideo should be false for CANVAS", canvasParams.isVideo());

        ContextMenuParams noneParams =
                createContextMenuParamsWithMediaType(ContextMenuDataMediaType.NONE);
        assertFalse("isCanvas should be false for NONE", noneParams.isCanvas());
        assertTrue("isPage should be true for NONE", noneParams.isPage());

        ContextMenuParams imageParams =
                createContextMenuParamsWithMediaType(ContextMenuDataMediaType.IMAGE);
        assertFalse("isCanvas should be false for IMAGE", imageParams.isCanvas());
        assertTrue("isImage should be true for IMAGE", imageParams.isImage());

        ContextMenuParams videoParams =
                createContextMenuParamsWithMediaType(ContextMenuDataMediaType.VIDEO);
        assertFalse("isCanvas should be false for VIDEO", videoParams.isCanvas());
        assertTrue("isVideo should be true for VIDEO", videoParams.isVideo());
    }

    @Test
    public void testHasImageContents() {
        ContextMenuParams defaultCanvasParams =
                createContextMenuParamsWithMediaType(ContextMenuDataMediaType.CANVAS);
        assertTrue(
                "hasImageContents should default to true for CANVAS in test ctor",
                defaultCanvasParams.hasImageContents());

        ContextMenuParams defaultImageParams =
                createContextMenuParamsWithMediaType(ContextMenuDataMediaType.IMAGE);
        assertTrue(
                "hasImageContents should default to true for IMAGE in test ctor",
                defaultImageParams.hasImageContents());

        ContextMenuParams defaultNoneParams =
                createContextMenuParamsWithMediaType(ContextMenuDataMediaType.NONE);
        assertFalse(
                "hasImageContents should default to false for NONE in test ctor",
                defaultNoneParams.hasImageContents());

        ContextMenuParams defaultVideoParams =
                createContextMenuParamsWithMediaType(ContextMenuDataMediaType.VIDEO);
        assertFalse(
                "hasImageContents should default to false for VIDEO in test ctor",
                defaultVideoParams.hasImageContents());

        ContextMenuParams cleanCanvasParams =
                new ContextMenuParams(
                        /* nativePtr= */ null,
                        /* menuModelBridge= */ null,
                        ContextMenuDataMediaType.CANVAS,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
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
                        MenuSourceType.MOUSE,
                        /* openedFromHighlight= */ false,
                        /* openedFromInterestFor= */ false,
                        /* additionalNavigationParams= */ null,
                        /* hasImageContents= */ true);
        assertTrue("hasImageContents should be true", cleanCanvasParams.hasImageContents());

        ContextMenuParams taintedCanvasParams =
                new ContextMenuParams(
                        /* nativePtr= */ null,
                        /* menuModelBridge= */ null,
                        ContextMenuDataMediaType.CANVAS,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
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
                        MenuSourceType.MOUSE,
                        /* openedFromHighlight= */ false,
                        /* openedFromInterestFor= */ false,
                        /* additionalNavigationParams= */ null,
                        /* hasImageContents= */ false);
        assertFalse("hasImageContents should be false", taintedCanvasParams.hasImageContents());
    }
}
