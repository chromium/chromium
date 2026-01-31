// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;

import android.app.Activity;
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
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.AnchoredPopupWindow;

/** Unit tests for FuseboxPopup. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FuseboxPopupUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private @Mock AnchoredPopupWindow mPopupWindow;
    private @Mock View.AccessibilityDelegate mAccessibilityDelegate;

    private Activity mActivity;
    private FuseboxPopup mFuseboxPopup;
    private View mContentView;
    private ViewGroup mViewGroup;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(TestActivity.class);
        mContentView = LayoutInflater.from(mActivity).inflate(R.layout.fusebox_context_popup, null);
        mViewGroup = mContentView.findViewById(R.id.fusebox_view_group);
        mFuseboxPopup = new FuseboxPopup(mActivity, mPopupWindow, mContentView);
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
}
