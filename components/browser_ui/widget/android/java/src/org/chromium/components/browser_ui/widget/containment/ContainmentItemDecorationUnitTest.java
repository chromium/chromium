// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.containment;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.RippleDrawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.R;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link ContainmentItemDecoration}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContainmentItemDecorationUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private RecyclerView mRecyclerView;
    @Mock private RecyclerView.State mState;
    @Mock private Canvas mCanvas;
    @Mock private ContainmentItemController mController;

    private Context mContext;
    private ContainmentItemDecoration mDecoration;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContext = activity;

        mDecoration = new ContainmentItemDecoration(mController);
    }

    private ContainerStyle createTestStyle() {
        return new ContainerStyle.Builder()
                .setTopRadius(16f)
                .setBottomRadius(16f)
                .setBackgroundColor(Color.BLUE)
                .build();
    }

    private View createChildView() {
        View view = new View(mContext);
        view.setLayoutParams(
                new RecyclerView.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        return view;
    }

    @Test
    public void testOnDraw_nullPreferenceStyles_doesNotUpdate() {
        assertThat(mDecoration.getUpdateBackgroundsForTesting()).isTrue();

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);

        verify(mRecyclerView, never()).getChildCount();
        assertThat(mDecoration.getUpdateBackgroundsForTesting()).isTrue();
    }

    @Test
    public void testOnDraw_updateBackgroundsFalse_doesNotUpdate() {
        ArrayList<ContainerStyle> styles = new ArrayList<>(List.of(createTestStyle()));
        mDecoration.updatePreferenceStyles(styles);

        View child = createChildView();
        when(mRecyclerView.getChildCount()).thenReturn(1);
        when(mRecyclerView.getChildAt(0)).thenReturn(child);
        when(mRecyclerView.getChildAdapterPosition(child)).thenReturn(0);

        // First onDraw styles child and sets mUpdateBackgrounds to false.
        mDecoration.onDraw(mCanvas, mRecyclerView, mState);
        assertThat(child.getBackground()).isInstanceOf(RippleDrawable.class);
        assertThat(mDecoration.getUpdateBackgroundsForTesting()).isFalse();

        // Clear background and invocations.
        child.setBackground(null);
        clearInvocations(mRecyclerView);

        // Subsequent onDraw returns early because mUpdateBackgrounds is false.
        mDecoration.onDraw(mCanvas, mRecyclerView, mState);
        verify(mRecyclerView, never()).getChildCount();
        assertThat(child.getBackground()).isNull();
    }

    @Test
    public void testOnDraw_stylesVisibleChildren_clearsUpdateFlag() {
        ContainerStyle style0 = createTestStyle();
        ContainerStyle style1 = createTestStyle();
        ArrayList<ContainerStyle> styles = new ArrayList<>(List.of(style0, style1));
        mDecoration.updatePreferenceStyles(styles);

        View child0 = createChildView();
        View child1 = createChildView();

        when(mRecyclerView.getChildCount()).thenReturn(2);
        when(mRecyclerView.getChildAt(0)).thenReturn(child0);
        when(mRecyclerView.getChildAt(1)).thenReturn(child1);
        when(mRecyclerView.getChildAdapterPosition(child0)).thenReturn(0);
        when(mRecyclerView.getChildAdapterPosition(child1)).thenReturn(1);

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);

        assertThat(child0.getBackground()).isInstanceOf(RippleDrawable.class);
        assertThat(child1.getBackground()).isInstanceOf(RippleDrawable.class);
        assertThat(mDecoration.getUpdateBackgroundsForTesting()).isFalse();
    }

    @Test
    public void testOnDraw_clearsUpdateFlag_andStylesNewChildrenAfterGetItemOffsets() {
        ContainerStyle style0 = createTestStyle();
        ContainerStyle style1 = createTestStyle();
        ArrayList<ContainerStyle> styles = new ArrayList<>(List.of(style0, style1));
        mDecoration.updatePreferenceStyles(styles);

        View child0 = createChildView();
        View child1 = createChildView();

        // Initially only child0 is attached to RecyclerView, while adapter has 2 items.
        when(mRecyclerView.getChildCount()).thenReturn(1);
        when(mRecyclerView.getChildAt(0)).thenReturn(child0);
        when(mRecyclerView.getChildAdapterPosition(child0)).thenReturn(0);

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);

        // child0 was styled, and the update flag was cleared to avoid redrawing continuously.
        assertThat(child0.getBackground()).isInstanceOf(RippleDrawable.class);
        assertThat(child1.getBackground()).isNull();
        assertThat(mDecoration.getUpdateBackgroundsForTesting()).isFalse();

        // Later child1 attaches and its offsets are measured.
        Rect outRect = new Rect();
        mDecoration.getItemOffsets(outRect, child1, mRecyclerView, mState);
        assertThat(mDecoration.getUpdateBackgroundsForTesting()).isTrue();

        // Next draw pass styles child1.
        when(mRecyclerView.getChildCount()).thenReturn(2);
        when(mRecyclerView.getChildAt(0)).thenReturn(child0);
        when(mRecyclerView.getChildAt(1)).thenReturn(child1);
        when(mRecyclerView.getChildAdapterPosition(child0)).thenReturn(0);
        when(mRecyclerView.getChildAdapterPosition(child1)).thenReturn(1);

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);

        // Now child1 is also styled and the update flag is cleared.
        assertThat(child1.getBackground()).isInstanceOf(RippleDrawable.class);
        assertThat(mDecoration.getUpdateBackgroundsForTesting()).isFalse();
    }

    @Test
    public void testUpdatePreferenceStyles_resetsUpdateBackgroundsFlag() {
        ArrayList<ContainerStyle> styles = new ArrayList<>(List.of(createTestStyle()));
        mDecoration.updatePreferenceStyles(styles);

        View child = createChildView();
        when(mRecyclerView.getChildCount()).thenReturn(1);
        when(mRecyclerView.getChildAt(0)).thenReturn(child);
        when(mRecyclerView.getChildAdapterPosition(child)).thenReturn(0);

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);
        assertThat(mDecoration.getUpdateBackgroundsForTesting()).isFalse();

        mDecoration.updatePreferenceStyles(styles);
        assertThat(mDecoration.getUpdateBackgroundsForTesting()).isTrue();
    }

    @Test
    public void testGetContainerStyle() {
        assertThat(mDecoration.getContainerStyle(0)).isNull();

        ContainerStyle style0 = createTestStyle();
        ContainerStyle style1 = createTestStyle();
        mDecoration.updatePreferenceStyles(new ArrayList<>(List.of(style0, style1)));

        assertThat(mDecoration.getContainerStyle(0)).isEqualTo(style0);
        assertThat(mDecoration.getContainerStyle(1)).isEqualTo(style1);
        assertThat(mDecoration.getContainerStyle(-1)).isNull();
        assertThat(mDecoration.getContainerStyle(2)).isNull();
    }
}
