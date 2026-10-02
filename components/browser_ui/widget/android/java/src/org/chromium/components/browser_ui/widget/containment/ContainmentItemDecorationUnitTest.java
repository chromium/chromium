// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.containment;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.RippleDrawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.LinearLayoutManager;
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
    private static final int CHILD_HEIGHT_PX = 10;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private RecyclerView.State mState;
    @Mock private Canvas mCanvas;
    @Mock private ContainmentItemController mController;

    private Context mContext;
    private RecyclerView mRecyclerView;
    private ContainmentItemDecoration mDecoration;

    /** Adapter which serves a fixed list of pre-created child views. */
    private static class ViewListAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
        private final List<View> mViews;

        ViewListAdapter(List<View> views) {
            mViews = views;
        }

        @Override
        public int getItemCount() {
            return mViews.size();
        }

        @Override
        public int getItemViewType(int position) {
            return position;
        }

        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            return new RecyclerView.ViewHolder(mViews.get(viewType)) {};
        }

        @Override
        public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {}
    }

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContext = activity;
        mRecyclerView = new RecyclerView(mContext);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(mContext));

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
                        ViewGroup.LayoutParams.MATCH_PARENT, CHILD_HEIGHT_PX));
        return view;
    }

    /** Sets the adapter items to {@code views} and lays out the RecyclerView. */
    private void setItems(View... views) {
        mRecyclerView.setAdapter(new ViewListAdapter(List.of(views)));
        layoutRecyclerView(views.length * CHILD_HEIGHT_PX);
    }

    private void layoutRecyclerView(int heightPx) {
        mRecyclerView.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(heightPx, View.MeasureSpec.EXACTLY));
        mRecyclerView.layout(0, 0, 100, heightPx);
    }

    @Test
    public void testOnDraw_nullPreferenceStyles_doesNotUpdate() {
        View child = createChildView();
        setItems(child);
        assertThat(mDecoration.getUpdateBackgroundsForTesting()).isTrue();

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);

        assertThat(child.getBackground()).isNull();
        assertThat(mDecoration.getUpdateBackgroundsForTesting()).isTrue();
    }

    @Test
    public void testOnDraw_updateBackgroundsFalse_doesNotUpdate() {
        ArrayList<ContainerStyle> styles = new ArrayList<>(List.of(createTestStyle()));
        mDecoration.updatePreferenceStyles(styles);

        View child = createChildView();
        setItems(child);

        // First onDraw styles child and sets mUpdateBackgrounds to false.
        mDecoration.onDraw(mCanvas, mRecyclerView, mState);
        assertThat(child.getBackground()).isInstanceOf(RippleDrawable.class);
        assertThat(mDecoration.getUpdateBackgroundsForTesting()).isFalse();

        // Clear background.
        child.setBackground(null);

        // Subsequent onDraw returns early because mUpdateBackgrounds is false.
        mDecoration.onDraw(mCanvas, mRecyclerView, mState);
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

        setItems(child0, child1);

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
        mRecyclerView.setAdapter(new ViewListAdapter(List.of(child0, child1)));
        layoutRecyclerView(CHILD_HEIGHT_PX);
        assertThat(mRecyclerView.getChildCount()).isEqualTo(1);

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);

        // child0 was styled, and the update flag was cleared to avoid redrawing continuously.
        assertThat(child0.getBackground()).isInstanceOf(RippleDrawable.class);
        assertThat(child1.getBackground()).isNull();
        assertThat(mDecoration.getUpdateBackgroundsForTesting()).isFalse();

        // Later child1 attaches and its offsets are measured.
        layoutRecyclerView(2 * CHILD_HEIGHT_PX);
        assertThat(mRecyclerView.getChildCount()).isEqualTo(2);
        Rect outRect = new Rect();
        mDecoration.getItemOffsets(outRect, child1, mRecyclerView, mState);
        assertThat(mDecoration.getUpdateBackgroundsForTesting()).isTrue();

        // Next draw pass styles child1.
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
        setItems(child);

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
