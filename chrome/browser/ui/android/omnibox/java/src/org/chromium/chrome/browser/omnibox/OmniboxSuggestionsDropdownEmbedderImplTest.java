// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static junit.framework.Assert.assertEquals;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder.OmniboxAlignment;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowDelegate;

import java.lang.ref.WeakReference;

/**
 * Unit tests for {@link OmniboxSuggestionsDropdownEmbedderImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxSuggestionsDropdownEmbedderImplTest {
    private static final int ANCHOR_WIDTH = 600;
    private static final int ANCHOR_HEIGHT = 80;
    private static final int ANCHOR_TOP = 31;
    private static final int ALIGNMENT_WIDTH = 400;
    // Sentinel value for mistaken use of alignment view top instead of left. If you see a 43, it's
    // probably because you used position[1] instead of position[0].
    private static final int ALIGNMENT_TOP = 43;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock WindowAndroid mWindowAndroid;
    private @Mock WindowDelegate mWindowDelegate;
    private @Mock ViewTreeObserver mViewTreeObserver;
    private @Mock ViewGroup mContentView;
    private @Mock ViewGroup mAnchorView;
    private @Mock View mHorizontalAlignmentView;

    private OmniboxSuggestionsDropdownEmbedderImpl mImpl;
    private WeakReference<Context> mContextWeakRef;

    @Before
    public void setUp() {
        mContextWeakRef = new WeakReference<>(ContextUtils.getApplicationContext());
        doReturn(mContextWeakRef).when(mWindowAndroid).getContext();
        doReturn(mViewTreeObserver).when(mAnchorView).getViewTreeObserver();
        doReturn(mContentView).when(mAnchorView).getRootView();
        doReturn(mContentView).when(mContentView).findViewById(android.R.id.content);
        doReturn(mContentView).when(mAnchorView).getParent();
        doReturn(ANCHOR_WIDTH).when(mAnchorView).getMeasuredWidth();
        doReturn(ALIGNMENT_WIDTH).when(mHorizontalAlignmentView).getMeasuredWidth();
        doReturn(ANCHOR_HEIGHT).when(mAnchorView).getMeasuredHeight();
        doReturn(ANCHOR_TOP).when(mAnchorView).getTop();
        doReturn(ALIGNMENT_TOP).when(mHorizontalAlignmentView).getTop();
        mImpl = new OmniboxSuggestionsDropdownEmbedderImpl(
                mWindowAndroid, mWindowDelegate, mAnchorView, mHorizontalAlignmentView);
    }

    @Test
    public void testWindowAttachment() {
        verify(mAnchorView, never()).addOnLayoutChangeListener(mImpl);
        verify(mHorizontalAlignmentView, never()).addOnLayoutChangeListener(mImpl);
        verify(mAnchorView, never()).getViewTreeObserver();

        mImpl.onAttachedToWindow();

        verify(mAnchorView).addOnLayoutChangeListener(mImpl);
        verify(mHorizontalAlignmentView).addOnLayoutChangeListener(mImpl);
        verify(mViewTreeObserver).addOnGlobalLayoutListener(mImpl);

        mImpl.onDetachedFromWindow();
        verify(mAnchorView).removeOnLayoutChangeListener(mImpl);
        verify(mHorizontalAlignmentView).removeOnLayoutChangeListener(mImpl);
        verify(mViewTreeObserver).removeOnGlobalLayoutListener(mImpl);
    }

    @Test
    public void testRecalculateOmniboxAlignment_phone() {
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(40).when(mHorizontalAlignmentView).getLeft();
        doReturn(60).when(mHorizontalAlignmentView).getTop();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(
                new OmniboxAlignment(0, ANCHOR_HEIGHT + ANCHOR_TOP, ANCHOR_WIDTH, 0, 0), alignment);
    }

    @Test
    @Config(qualifiers = "w600dp-h820dp")
    public void testRecalculateOmniboxAlignment_tablet() {
        doReturn(mAnchorView).when(mHorizontalAlignmentView).getParent();
        doReturn(40).when(mHorizontalAlignmentView).getLeft();
        mImpl.recalculateOmniboxAlignment();
        OmniboxAlignment alignment = mImpl.getCurrentAlignment();
        assertEquals(new OmniboxAlignment(0, ANCHOR_HEIGHT + ANCHOR_TOP, ANCHOR_WIDTH, 40,
                             ANCHOR_WIDTH - ALIGNMENT_WIDTH - 40),
                alignment);
    }
}
