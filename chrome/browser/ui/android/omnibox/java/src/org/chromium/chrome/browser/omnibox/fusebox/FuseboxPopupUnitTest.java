// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.AnchoredPopupWindow;

/** Unit tests for FuseboxPopup. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FuseboxPopupUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private @Mock AnchoredPopupWindow mPopupWindow;
    private @Mock View.AccessibilityDelegate mAccessibilityDelegate;
    private @Mock DynamicRectProvider mDynamicRectProvider;

    private Activity mActivity;
    private FuseboxPopup mFuseboxPopup;
    private View mContentView;
    private ViewGroup mViewGroup;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(TestActivity.class);
        mContentView = LayoutInflater.from(mActivity).inflate(R.layout.fusebox_context_popup, null);
        mViewGroup = mContentView.findViewById(R.id.fusebox_view_group);
        mFuseboxPopup =
                new FuseboxPopup(
                        mActivity,
                        mPopupWindow,
                        mContentView,
                        mDynamicRectProvider,
                        /* isBottomSheet= */ false);
    }

    @Test
    public void testFocusFirstViewForAccessibility_focusableChild() {
        View v = mViewGroup.getChildAt(0);

        v.setVisibility(View.VISIBLE);
        v.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        v.setAccessibilityDelegate(mAccessibilityDelegate);

        mFuseboxPopup.focusFirstViewForAccessibility();

        verify(mAccessibilityDelegate, atLeastOnce())
                .sendAccessibilityEvent(v, AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    @Test
    public void testFocusFirstViewForAccessibility_unimportantChild() {
        View v1 = mViewGroup.getChildAt(0);
        View v2 = mViewGroup.getChildAt(1);

        v1.setVisibility(View.VISIBLE);
        v1.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        v1.setAccessibilityDelegate(mAccessibilityDelegate);

        v2.setVisibility(View.VISIBLE);
        v2.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        v2.setAccessibilityDelegate(mAccessibilityDelegate);

        mFuseboxPopup.focusFirstViewForAccessibility();

        verify(mAccessibilityDelegate, atLeastOnce())
                .sendAccessibilityEvent(v2, AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    @Test
    public void testFocusFirstViewForAccessibility_invisibleChild() {
        View v1 = mViewGroup.getChildAt(0);
        View v2 = mViewGroup.getChildAt(1);

        v1.setVisibility(View.GONE);
        v1.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        v1.setAccessibilityDelegate(mAccessibilityDelegate);

        v2.setVisibility(View.VISIBLE);
        v2.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        v2.setAccessibilityDelegate(mAccessibilityDelegate);

        mFuseboxPopup.focusFirstViewForAccessibility();

        verify(mAccessibilityDelegate, atLeastOnce())
                .sendAccessibilityEvent(v2, AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    @Test
    public void testSetPopupState_Hidden() {
        mFuseboxPopup.setPopupState(FuseboxProperties.PopupState.HIDDEN);
        verify(mDynamicRectProvider).setPopupState(FuseboxProperties.PopupState.HIDDEN);
        verify(mPopupWindow).dismiss();
    }

    @Test
    public void testSetPopupState_Floating() {
        mFuseboxPopup.setPopupState(FuseboxProperties.PopupState.FLOATING);
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        verify(mDynamicRectProvider).setPopupState(FuseboxProperties.PopupState.FLOATING);
        verify(mPopupWindow).show();
    }

    @Test
    public void testSetPopupState_Bottom() {
        mFuseboxPopup.setPopupState(FuseboxProperties.PopupState.BOTTOM);
        verify(mDynamicRectProvider).setPopupState(FuseboxProperties.PopupState.BOTTOM);
        verify(mPopupWindow).show();
    }

    @Test
    public void testSetPopupState_Bottom_setsAnimation() {
        mFuseboxPopup.setPopupState(FuseboxProperties.PopupState.BOTTOM);
        verify(mPopupWindow).setAnimationStyle(R.style.FuseboxBottomSheetAnimation);
    }

    @Test
    public void testSetPopupState_Floating_clearsAnimation() {
        mFuseboxPopup.setPopupState(FuseboxProperties.PopupState.FLOATING);
        verify(mPopupWindow).setAnimationStyle(0);
    }

    @Test
    public void testDynamicInflation_VerticalLayout() {
        OmniboxFeatures.sShowBottomSheetPopup.setForTesting(false);

        // Re-create content view and popup to trigger new inflation logic
        mContentView = LayoutInflater.from(mActivity).inflate(R.layout.fusebox_context_popup, null);
        mFuseboxPopup =
                new FuseboxPopup(
                        mActivity, mPopupWindow, mContentView, mDynamicRectProvider, false);

        // Verify that we can find the elements
        assertNotNull(mFuseboxPopup.mAddCurrentTab);
        assertNotNull(mFuseboxPopup.mTabButton);
        assertNotNull(mFuseboxPopup.mClipboardButton);
        assertNotNull(mFuseboxPopup.mCameraButton);
        assertNotNull(mFuseboxPopup.mGalleryButton);
        assertNotNull(mFuseboxPopup.mFileButton);
    }

    @Test
    public void testDynamicInflation_HorizontalLayout() {
        OmniboxFeatures.sShowBottomSheetPopup.setForTesting(true);

        // Re-create content view and popup to trigger new inflation logic
        mContentView = LayoutInflater.from(mActivity).inflate(R.layout.fusebox_context_popup, null);
        mFuseboxPopup =
                new FuseboxPopup(
                        mActivity,
                        mPopupWindow,
                        mContentView,
                        mDynamicRectProvider,
                        /* isBottomSheet= */ true);

        // Verify that we can find the elements
        assertNotNull(mFuseboxPopup.mAddCurrentTab);
        assertNotNull(mFuseboxPopup.mTabButton);
        assertNotNull(mFuseboxPopup.mClipboardButton);
        assertNotNull(mFuseboxPopup.mCameraButton);
        assertNotNull(mFuseboxPopup.mGalleryButton);
        assertNotNull(mFuseboxPopup.mFileButton);
    }

    @Test
    public void testUpdateLayout() {
        doReturn(100)
                .when(mDynamicRectProvider)
                .getPopupWidth(eq(FuseboxProperties.PopupState.FLOATING), any());

        doReturn(true).when(mPopupWindow).isShowing();

        mFuseboxPopup.setPopupState(FuseboxProperties.PopupState.FLOATING);

        Shadows.shadowOf(Looper.getMainLooper()).idle();

        verify(mPopupWindow, atLeastOnce()).updateDesiredContentSize(100, 0, true);
    }
}
