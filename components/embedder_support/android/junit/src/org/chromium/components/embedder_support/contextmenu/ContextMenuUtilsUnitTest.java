// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.app.Activity;
import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.view.DragEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.webkit.URLUtil;

import androidx.test.filters.SmallTest;

import org.junit.After;
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
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink_public.common.ContextMenuDataMediaFlags;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.components.embedder_support.contextmenu.ContextMenuUtils.HeaderInfo;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.dragdrop.DragStateTracker;
import org.chromium.ui.listmenu.MenuModelBridge;
import org.chromium.ui.mojom.MenuSourceType;
import org.chromium.url.GURL;

/** Unit tests for {@link ContextMenuUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextMenuUtilsUnitTest {
    Activity mActivity;
    @Mock WebContents mWebContentsMock;
    private static final String sTitleText = "titleText";
    private static final String sLinkText = "linkText";
    private static final String sSrcUrl = "https://www.google.com/";
    private static final GURL sPageGUrl = new GURL("https://www.youtube.com/");
    private static final GURL sSrcGUrl = new GURL("https://www.google.com/");
    private static final GURL sLinkGUrl = new GURL("https://www.wikipedia.org/");

    @Mock private MenuModelBridge mMenuModelBridge;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        MockitoAnnotations.openMocks(this);
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    @Test
    @SmallTest
    public void testGetHeaderInfo_noCustomItemPresent() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        sPageGUrl,
                        sLinkGUrl,
                        sLinkText,
                        GURL.emptyGURL(),
                        sSrcGUrl,
                        sTitleText,
                        null,
                        true,
                        0,
                        0,
                        0,
                        false,
                        false,
                        0,
                        null);

        HeaderInfo headerInfo =
                ContextMenuUtils.getHeaderInfo(
                        params,
                        /** isCustomContextMenuItemPresent= */
                        false);

        assertEquals("Title should be the default title.", sTitleText, headerInfo.getTitle());
        assertEquals("URL should be the link URL.", sLinkGUrl, headerInfo.getUrl());
        assertEquals(
                "Secondary URL should be empty.", GURL.emptyGURL(), headerInfo.getSecondaryUrl());
        assertEquals(
                "Tertiary URL should be empty.", GURL.emptyGURL(), headerInfo.getTertiaryUrl());
    }

    @Test
    @SmallTest
    public void testGetHeaderInfo_customItemPresent_notImage() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.VIDEO,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        sPageGUrl,
                        sLinkGUrl,
                        sLinkText,
                        GURL.emptyGURL(),
                        sSrcGUrl,
                        sTitleText,
                        null,
                        true,
                        0,
                        0,
                        0,
                        false,
                        false,
                        0,
                        null);

        HeaderInfo headerInfo =
                ContextMenuUtils.getHeaderInfo(
                        params,
                        /** isCustomContextMenuItemPresent= */
                        true);

        assertEquals("Title should be the default title.", sTitleText, headerInfo.getTitle());
        assertEquals(
                "URL should be the link URL as it's not an image.", sLinkGUrl, headerInfo.getUrl());
        assertEquals(
                "Secondary URL should be empty as it's not an image.",
                GURL.emptyGURL(),
                headerInfo.getSecondaryUrl());
        assertEquals(
                "Tertiary URL should be empty as it's not an image.",
                GURL.emptyGURL(),
                headerInfo.getTertiaryUrl());
    }

    @Test
    @SmallTest
    public void testGetHeaderInfo_customItemPresent_isImageLink() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        sPageGUrl,
                        sLinkGUrl,
                        sLinkText,
                        GURL.emptyGURL(),
                        sSrcGUrl,
                        sTitleText,
                        null,
                        true,
                        0,
                        0,
                        0,
                        false,
                        false,
                        0,
                        null);

        HeaderInfo headerInfo =
                ContextMenuUtils.getHeaderInfo(
                        params,
                        /** isCustomContextMenuItemPresent= */
                        true);

        assertEquals("Title should be the default title.", sTitleText, headerInfo.getTitle());
        assertEquals("URL should be the src URL.", sSrcGUrl, headerInfo.getUrl());
        assertEquals(
                "Secondary URL should be the page URL.", sPageGUrl, headerInfo.getSecondaryUrl());
        assertEquals(
                "Tertiary URL should be the link URL.", sLinkGUrl, headerInfo.getTertiaryUrl());
    }

    @Test
    @SmallTest
    public void testGetHeaderInfo_customItemPresent_isImageNotAnchor() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        sPageGUrl,
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        sSrcGUrl,
                        sTitleText,
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        false,
                        0,
                        null);

        HeaderInfo headerInfo =
                ContextMenuUtils.getHeaderInfo(
                        params,
                        /** isCustomContextMenuItemPresent= */
                        true);

        assertEquals("Title should be the default title.", sTitleText, headerInfo.getTitle());
        assertEquals("URL should be the src URL.", sSrcGUrl, headerInfo.getUrl());
        assertEquals(
                "Secondary URL should be the page URL.", sPageGUrl, headerInfo.getSecondaryUrl());
        assertEquals(
                "Tertiary URL should be empty.", GURL.emptyGURL(), headerInfo.getTertiaryUrl());
    }

    @Test
    @SmallTest
    public void getTitle_hasTitleText() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        sLinkText,
                        GURL.emptyGURL(),
                        new GURL(sSrcUrl),
                        sTitleText,
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        assertEquals(sTitleText, ContextMenuUtils.getTitle(params));
    }

    @Test
    @SmallTest
    public void getTitle_noTitleTextHasLinkText() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        sLinkText,
                        GURL.emptyGURL(),
                        new GURL(sSrcUrl),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        assertEquals(sLinkText, ContextMenuUtils.getTitle(params));
    }

    @Test
    @SmallTest
    public void getTitle_noTitleTextOrLinkText() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.IMAGE,
                        0,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        new GURL(sSrcUrl),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        assertEquals(URLUtil.guessFileName(sSrcUrl, null, null), ContextMenuUtils.getTitle(params));
    }

    @Test
    @SmallTest
    public void getTitle_noShareParams() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        "",
                        null,
                        false,
                        0,
                        0,
                        0,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        assertEquals("", ContextMenuUtils.getTitle(params));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void usePopupAllScreen_Small() {
        doTestUsePopupWhenEnabledByFlag();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @CommandLineFlags.Add(ContextMenuSwitches.FORCE_CONTEXT_MENU_POPUP)
    public void usePopupAllScreen_Large() {
        doTestUsePopupWhenEnabledByFlag();
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void doNotUsePopupForSmallScreen() {
        assertFalse(
                "Popup should not be used for small screen.",
                ContextMenuUtils.isPopupSupported(mActivity));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void usePopupForLargeScreen() {
        assertTrue(
                "Popup should be used for large screen.",
                ContextMenuUtils.isPopupSupported(mActivity));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void nullInputs() {
        assertFalse("Always return false for null input.", ContextMenuUtils.isPopupSupported(null));
    }

    private void doTestUsePopupWhenEnabledByFlag() {
        assertTrue(
                "Popup should be used when switch FORCE_CONTEXT_MENU_POPUP is enabled.",
                ContextMenuUtils.isPopupSupported(mActivity));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw320dp")
    @DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void isDragDropEnabled_featureDisabledPopupNotSupported() {
        assertFalse(
                "Should return false if the feature is disabled and the popup is not supported.",
                ContextMenuUtils.isDragDropEnabled(mActivity));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw320dp")
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void isDragDropEnabled_featureEnabledPopupNotSupported() {
        assertFalse(
                "Should return false if the popup is not supported.",
                ContextMenuUtils.isDragDropEnabled(mActivity));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    @DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void isDragDropEnabled_featureDisabledPopupSupported() {
        assertFalse(
                "Should return false if the feature is disabled.",
                ContextMenuUtils.isDragDropEnabled(mActivity));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void isDragDropEnabled_featureEnabledPopupSupported() {
        assertTrue(
                "Should return true if the feature is enabled and the popup is supported.",
                ContextMenuUtils.isDragDropEnabled(mActivity));
    }

    @Test
    @SmallTest
    public void isMouseOrHighlightPopup_mouse() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        sLinkText,
                        GURL.emptyGURL(),
                        new GURL(sSrcUrl),
                        sTitleText,
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.MOUSE,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        assertTrue(
                "Should return true if source type is MOUSE.",
                ContextMenuUtils.isMouseOrHighlightPopup(params));
    }

    @Test
    @SmallTest
    public void isMouseOrHighlightPopup_highlight() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        sLinkText,
                        GURL.emptyGURL(),
                        new GURL(sSrcUrl),
                        sTitleText,
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.NONE,
                        true, // openedFromHighlight
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        assertTrue(
                "Should return true if opened from highlight.",
                ContextMenuUtils.isMouseOrHighlightPopup(params));
    }

    @Test
    @SmallTest
    public void isMouseOrHighlightPopup_neither() {
        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        sLinkText,
                        GURL.emptyGURL(),
                        new GURL(sSrcUrl),
                        sTitleText,
                        null,
                        false,
                        0,
                        0,
                        MenuSourceType.NONE,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        assertFalse(
                "Should return false if neither mouse nor highlight.",
                ContextMenuUtils.isMouseOrHighlightPopup(params));
    }

    @Test
    @SmallTest
    public void testGetTouchPointCoordinates_popupWindow() {
        doTestGetTouchPointCoordinates(true);
    }

    @Test
    @SmallTest
    public void testGetTouchPointCoordinates_dialog() {
        doTestGetTouchPointCoordinates(false);
    }

    private void doTestGetTouchPointCoordinates(boolean isPopup) {
        Context context = mActivity;

        // In a real Android environment, the containerView would be part of a
        // layout hierarchy that's been inflated and attached to a Window.
        // However, in our JUnit tests, we don't have a real layout, so we need
        // to mock the containerView and its behavior.
        View mockContainerView = mock(View.class);
        Window mockWindow = mock(Window.class);
        WindowManager.LayoutParams mockLayoutParams = mock(WindowManager.LayoutParams.class);

        int[] mockLocation = {200, 300};
        // Simulate the behavior of View.getLocationOnScreen(), which populates the passed-in int
        // array with the view's screen coordinates.
        Mockito.doAnswer(
                        invocation -> {
                            int[] location = invocation.getArgument(0);
                            location[0] = mockLocation[0];
                            location[1] = mockLocation[1];
                            return null;
                        })
                .when(mockContainerView)
                .getLocationOnScreen(Mockito.any(int[].class));

        mockLayoutParams.x = 10;
        mockLayoutParams.y = 20;

        Mockito.when(mockWindow.getAttributes()).thenReturn(mockLayoutParams);

        int triggeringTouchXDp = 100;
        int triggeringTouchYDp = 200;
        float topContentOffsetPx = 50f;

        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        sLinkText,
                        GURL.emptyGURL(),
                        new GURL(sSrcUrl),
                        sTitleText,
                        null,
                        false,
                        triggeringTouchXDp,
                        triggeringTouchYDp,
                        MenuSourceType.NONE,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        Point result =
                ContextMenuUtils.getTouchPointCoordinates(
                        context,
                        mockWindow,
                        params,
                        topContentOffsetPx,
                        isPopup,
                        mockContainerView);

        // We need to check if the X and Y have been correctly modified by the method.
        final float density = context.getResources().getDisplayMetrics().density;
        final float touchPointXPx = triggeringTouchXDp * density;
        final float touchPointYPx = triggeringTouchYDp * density;
        int expectedX = (int) touchPointXPx;
        int expectedY = (int) (touchPointYPx + topContentOffsetPx);

        if (isPopup) {
            expectedX += mockLocation[0];
            expectedY += mockLocation[1];

            expectedX += mockLayoutParams.x;
            expectedY += mockLayoutParams.y;
        }

        assertEquals(expectedX, result.x);
        assertEquals(expectedY, result.y);
    }

    @Test
    @SmallTest
    public void testComputeDragShadowRect_dragStarted() {
        doTestComputeDragShadowRect(true);
    }

    @Test
    @SmallTest
    public void testComputeDragShadowRect_dragNotStarted() {
        doTestComputeDragShadowRect(false);
    }

    private void doTestComputeDragShadowRect(boolean isDragStarted) {
        int dragShadowWidth = 60;
        int dragShadowHeight = 50;
        int centerX = 150;
        int centerY = 250;

        setupMocksForDragShadowImage(isDragStarted, dragShadowWidth, dragShadowHeight);
        Rect result = ContextMenuUtils.computeDragShadowRect(mWebContentsMock, centerX, centerY);

        // Calculate the expected rect based on width, height, and center coordinates.
        int expectedLeft = centerX - dragShadowWidth / 2;
        int expectedTop = centerY - dragShadowHeight / 2;
        int expectedRight = centerX + dragShadowWidth / 2;
        int expectedBottom = centerY + dragShadowHeight / 2;

        Rect expected =
                isDragStarted
                        ? new Rect(expectedLeft, expectedTop, expectedRight, expectedBottom)
                        : new Rect(centerX, centerY, centerX, centerY);

        assertEquals(expected, result);
    }

    @Test
    @SmallTest
    @EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void testGetContextMenuAnchorRect_dragDropEnabled() {
        doTestGetContextMenuAnchorRect(true);
    }

    @Test
    @SmallTest
    @DisableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
    public void testGetContextMenuAnchorRect_dragDropNotEnabled() {
        doTestGetContextMenuAnchorRect(false);
    }

    public void doTestGetContextMenuAnchorRect(boolean isDragDropEnabled) {
        Context context = mActivity;
        Window window = mActivity.getWindow();
        View containerView = new View(context);
        int triggeringTouchXDp = 100;
        int triggeringTouchYDp = 200;
        float topContentOffsetPx = 50f;

        int shadowWidth = 60;
        int shadowHeight = 50;
        setupMocksForDragShadowImage(isDragDropEnabled, shadowWidth, shadowHeight);

        ContextMenuParams params =
                new ContextMenuParams(
                        0,
                        mMenuModelBridge,
                        ContextMenuDataMediaType.NONE,
                        ContextMenuDataMediaFlags.MEDIA_NONE,
                        GURL.emptyGURL(),
                        GURL.emptyGURL(),
                        sLinkText,
                        GURL.emptyGURL(),
                        new GURL(sSrcUrl),
                        sTitleText,
                        null,
                        false,
                        triggeringTouchXDp,
                        triggeringTouchYDp,
                        MenuSourceType.NONE,
                        false,
                        /* openedFromInterestFor= */ false,
                        /* interestForNodeID= */ 0,
                        /* additionalNavigationParams= */ null);

        // getContextMenuAnchorRect depends on static methods, and since we can't create static
        // mocks in Junit, we have to test the sub components of the method.
        Point touchPoint =
                ContextMenuUtils.getTouchPointCoordinates(
                        context,
                        window,
                        params,
                        topContentOffsetPx,
                        isDragDropEnabled /*usePopupWindow but should not matter for this case*/,
                        containerView);

        Rect result;
        if (isDragDropEnabled) {
            result =
                    ContextMenuUtils.computeDragShadowRect(
                            mWebContentsMock, touchPoint.x, touchPoint.y);
        } else {
            result = new Rect(touchPoint.x, touchPoint.y, touchPoint.x, touchPoint.y);
        }

        float density = context.getResources().getDisplayMetrics().density;
        float touchPointXPx = triggeringTouchXDp * density;
        float touchPointYPx = triggeringTouchYDp * density;
        int expectedX = (int) touchPointXPx;
        int expectedY = (int) (touchPointYPx + topContentOffsetPx);

        if (isDragDropEnabled) {
            int left = expectedX - shadowWidth / 2;
            int right = expectedX + shadowWidth / 2;
            int top = expectedY - shadowHeight / 2;
            int bottom = expectedY + shadowHeight / 2;
            Assert.assertEquals(
                    "Rect should be anchored next to the drag shadow.",
                    new Rect(left, top, right, bottom),
                    result);
        } else {
            Assert.assertEquals(
                    "Rect should be a single point.",
                    new Rect(expectedX, expectedY, expectedX, expectedY),
                    result);
        }
    }

    private void setupMocksForDragShadowImage(
            boolean isDragging, int dragShadowWidth, int dragShadowHeight) {
        ViewAndroidDelegate viewAndroidDelegate = Mockito.mock(ViewAndroidDelegate.class);
        DragStateTracker dragStateTracker =
                new DragStateTracker() {
                    @Override
                    public boolean onDrag(View v, DragEvent event) {
                        return false;
                    }

                    @Override
                    public boolean isDragStarted() {
                        return isDragging;
                    }

                    @Override
                    public int getDragShadowWidth() {
                        return dragShadowWidth;
                    }

                    @Override
                    public int getDragShadowHeight() {
                        return dragShadowHeight;
                    }
                };

        doReturn(viewAndroidDelegate).when(mWebContentsMock).getViewAndroidDelegate();
        doReturn(dragStateTracker).when(viewAndroidDelegate).getDragStateTracker();
    }
}
