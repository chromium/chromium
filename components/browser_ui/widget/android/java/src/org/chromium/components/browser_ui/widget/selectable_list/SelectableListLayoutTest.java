// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.selectable_list;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.ui.base.UiAndroidFeatures;
import org.chromium.ui.base.ViewUtils;

/** Tests for the wide display padding of {@link SelectableListLayout}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SelectableListLayoutTest {
    // Pixel Tablet sized window. A non-1 density catches dp/px mixups.
    private static final String LANDSCAPE_QUALIFIERS = "w1280dp-h800dp-land-xhdpi";
    private static final String PORTRAIT_QUALIFIERS = "w800dp-h1280dp-port-xhdpi";
    private static final int LONG_SIDE_DP = 1280;
    private static final int SHORT_SIDE_DP = 800;
    private static final int ACTION_ITEM_COUNT = 3;

    private Activity mActivity;
    private SelectableListLayout<Object> mLayout;
    private SelectableListToolbar<Object> mToolbar;
    private RecyclerView mRecyclerView;
    private TestAdapter mAdapter;

    /** An adapter with a mutable number of empty items. */
    private static class TestAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
        private int mItemCount;

        TestAdapter(int itemCount) {
            mItemCount = itemCount;
        }

        void setItemCount(int itemCount) {
            mItemCount = itemCount;
        }

        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            View view = new View(parent.getContext());
            view.setLayoutParams(
                    new RecyclerView.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT, dpToPx(/* dp= */ 48)));
            return new RecyclerView.ViewHolder(view) {};
        }

        @Override
        public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {}

        @Override
        public int getItemCount() {
            return mItemCount;
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    @Config(qualifiers = LANDSCAPE_QUALIFIERS)
    @EnableFeatures(UiAndroidFeatures.UPDATE_PADDING_FOR_DISPLAY_CALCULATION)
    public void rotateLandscapeToPortrait_toolbarTitleRemainsVisible() {
        rotateLandscapeToPortraitAndAssertTitleVisible();
    }

    @Test
    @Config(qualifiers = LANDSCAPE_QUALIFIERS)
    @DisableFeatures(UiAndroidFeatures.UPDATE_PADDING_FOR_DISPLAY_CALCULATION)
    public void rotateLandscapeToPortrait_flagDisabled_toolbarTitleRemainsVisible() {
        rotateLandscapeToPortraitAndAssertTitleVisible();
    }

    /**
     * Padding confidence check only. The content width between the paddings is 600dp in both
     * orientations, so children measured with stale padding are indistinguishable in this
     * direction; the landscape to portrait tests cover that.
     */
    @Test
    @Config(qualifiers = PORTRAIT_QUALIFIERS)
    @EnableFeatures(UiAndroidFeatures.UPDATE_PADDING_FOR_DISPLAY_CALCULATION)
    public void rotatePortraitToLandscape_paddingUpdated() {
        initializeLayout(/* itemCount= */ 1);
        measureAndLayout(/* widthDp= */ SHORT_SIDE_DP, /* heightDp= */ LONG_SIDE_DP);
        assertToolbarPadding(expectedPaddingDp(SHORT_SIDE_DP));

        rotate(LANDSCAPE_QUALIFIERS);
        measureAndLayout(/* widthDp= */ LONG_SIDE_DP, /* heightDp= */ SHORT_SIDE_DP);

        assertToolbarPadding(expectedPaddingDp(LONG_SIDE_DP));
        assertEquals(dpToPx(expectedPaddingDp(LONG_SIDE_DP)), mRecyclerView.getPaddingStart());
    }

    @Test
    @Config(qualifiers = LANDSCAPE_QUALIFIERS)
    @EnableFeatures(UiAndroidFeatures.UPDATE_PADDING_FOR_DISPLAY_CALCULATION)
    public void containerShrinksWithoutConfigChange_paddingUsesContainerWidth() {
        int shrunkWidthDp = 900;
        initializeLayout(/* itemCount= */ 1);
        measureAndLayout(/* widthDp= */ LONG_SIDE_DP, /* heightDp= */ SHORT_SIDE_DP);
        assertToolbarPadding(expectedPaddingDp(LONG_SIDE_DP));

        // Simulates side UI (e.g. a side panel) taking up part of the window.
        measureAndLayout(shrunkWidthDp, /* heightDp= */ SHORT_SIDE_DP);

        assertToolbarPadding(expectedPaddingDp(shrunkWidthDp));
        assertEquals(dpToPx(expectedPaddingDp(shrunkWidthDp)), mRecyclerView.getPaddingStart());
        assertTitleVisibleAndConverged(shrunkWidthDp, /* heightDp= */ SHORT_SIDE_DP);
    }

    @Test
    @Config(qualifiers = LANDSCAPE_QUALIFIERS)
    @EnableFeatures(UiAndroidFeatures.UPDATE_PADDING_FOR_DISPLAY_CALCULATION)
    public void emptyList_recyclerViewPaddingUsesContainerWidth() {
        initializeLayout(/* itemCount= */ 1);
        measureAndLayout(/* widthDp= */ LONG_SIDE_DP, /* heightDp= */ SHORT_SIDE_DP);
        assertEquals(dpToPx(expectedPaddingDp(LONG_SIDE_DP)), mRecyclerView.getPaddingStart());

        // Empty the list so the RecyclerView is gone, and is no longer measured, while rotating.
        mAdapter.setItemCount(0);
        mAdapter.notifyItemRemoved(0);
        assertEquals(View.GONE, mRecyclerView.getVisibility());

        rotate(PORTRAIT_QUALIFIERS);
        measureAndLayout(/* widthDp= */ SHORT_SIDE_DP, /* heightDp= */ LONG_SIDE_DP);

        mAdapter.setItemCount(1);
        mAdapter.notifyItemInserted(0);
        assertEquals(View.VISIBLE, mRecyclerView.getVisibility());
        measureAndLayout(/* widthDp= */ SHORT_SIDE_DP, /* heightDp= */ LONG_SIDE_DP);

        assertEquals(dpToPx(expectedPaddingDp(SHORT_SIDE_DP)), mRecyclerView.getPaddingStart());
        assertEquals(dpToPx(expectedPaddingDp(SHORT_SIDE_DP)), mRecyclerView.getPaddingEnd());
    }

    @Test
    @Config(qualifiers = LANDSCAPE_QUALIFIERS)
    @EnableFeatures(UiAndroidFeatures.UPDATE_PADDING_FOR_DISPLAY_CALCULATION)
    public void layoutHasHorizontalPadding_recyclerViewPaddingMatchesToolbar() {
        // E.g. system window insets applied as padding when fitsSystemWindows is set.
        int layoutPaddingDp = 40;
        int innerWidthDp = LONG_SIDE_DP - 2 * layoutPaddingDp;
        initializeLayout(/* itemCount= */ 1);
        mLayout.setPadding(dpToPx(layoutPaddingDp), 0, dpToPx(layoutPaddingDp), 0);

        measureAndLayout(/* widthDp= */ LONG_SIDE_DP, /* heightDp= */ SHORT_SIDE_DP);

        assertToolbarPadding(expectedPaddingDp(innerWidthDp));
        assertEquals(dpToPx(expectedPaddingDp(innerWidthDp)), mRecyclerView.getPaddingStart());
        assertEquals(dpToPx(expectedPaddingDp(innerWidthDp)), mRecyclerView.getPaddingEnd());
    }

    private void initializeLayout(int itemCount) {
        @SuppressWarnings("unchecked")
        SelectableListLayout<Object> layout =
                (SelectableListLayout<Object>)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.selectable_list_layout_test, /* root= */ null);
        mLayout = layout;
        mAdapter = new TestAdapter(itemCount);
        mRecyclerView = mLayout.initializeRecyclerView(mAdapter);
        // Bookmarks in a folder shows a back button, which leaves less room for the title.
        mToolbar =
                mLayout.initializeToolbar(
                        R.layout.selectable_list_toolbar_test,
                        new SelectionDelegate<>(),
                        R.string.test_toolbar_title,
                        /* normalGroupResId= */ 0,
                        /* selectedGroupResId= */ 0,
                        /* listener= */ null,
                        /* updateStatusBarColor= */ false,
                        /* menuResId= */ 0,
                        /* showBackInNormalView= */ true);
        for (int i = 0; i < ACTION_ITEM_COUNT; i++) {
            mToolbar.getMenu()
                    .add(Menu.NONE, Menu.NONE, Menu.NONE, "Action " + i)
                    .setIcon(R.drawable.test_ic_more_vert_black_24dp)
                    .setShowAsAction(MenuItem.SHOW_AS_ACTION_ALWAYS);
        }
        // Shows the RecyclerView if the adapter has items.
        mAdapter.notifyDataSetChanged();
        mLayout.configureWideDisplayStyle();
    }

    private void rotateLandscapeToPortraitAndAssertTitleVisible() {
        initializeLayout(/* itemCount= */ 1);
        measureAndLayout(/* widthDp= */ LONG_SIDE_DP, /* heightDp= */ SHORT_SIDE_DP);
        assertToolbarPadding(expectedPaddingDp(LONG_SIDE_DP));

        rotate(PORTRAIT_QUALIFIERS);
        measureAndLayout(/* widthDp= */ SHORT_SIDE_DP, /* heightDp= */ LONG_SIDE_DP);

        assertToolbarPadding(expectedPaddingDp(SHORT_SIDE_DP));
        assertEquals(dpToPx(expectedPaddingDp(SHORT_SIDE_DP)), mRecyclerView.getPaddingStart());
        assertTitleVisibleAndConverged(/* widthDp= */ SHORT_SIDE_DP, /* heightDp= */ LONG_SIDE_DP);
    }

    /** Replaces the full qualifier set, so {@code qualifiers} must include the density. */
    private void rotate(String qualifiers) {
        RuntimeEnvironment.setQualifiers(qualifiers);
        mLayout.dispatchConfigurationChanged(mActivity.getResources().getConfiguration());
    }

    /** Measures and lays out the (detached) layout, the same way a single traversal would. */
    private void measureAndLayout(int widthDp, int heightDp) {
        int widthPx = dpToPx(widthDp);
        int heightPx = dpToPx(heightDp);
        mLayout.measure(
                MeasureSpec.makeMeasureSpec(widthPx, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(heightPx, MeasureSpec.EXACTLY));
        mLayout.layout(0, 0, widthPx, heightPx);
    }

    private void assertToolbarPadding(int paddingDp) {
        int navButtonStartOffsetPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.selectable_list_toolbar_nav_button_start_offset);
        int endOffsetPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.selectable_list_search_icon_end_padding);
        assertEquals(dpToPx(paddingDp) + navButtonStartOffsetPx, mToolbar.getPaddingStart());
        assertEquals(dpToPx(paddingDp) + endOffsetPx, mToolbar.getPaddingEnd());
    }

    /**
     * Asserts that the title has a non-zero width, and that a clean measure pass with the current
     * padding does not change it, i.e. the title was measured with the padding it is laid out with.
     */
    private void assertTitleVisibleAndConverged(int widthDp, int heightDp) {
        TextView title = findTitleView();
        int titleWidth = title.getMeasuredWidth();
        assertTrue("Title should have a non-zero width.", titleWidth > 0);

        mToolbar.forceLayout();
        mLayout.forceLayout();
        measureAndLayout(widthDp, heightDp);
        assertEquals(
                "Title was measured with stale padding.", titleWidth, title.getMeasuredWidth());
    }

    private TextView findTitleView() {
        CharSequence titleText = mActivity.getString(R.string.test_toolbar_title);
        for (int i = 0; i < mToolbar.getChildCount(); i++) {
            if (mToolbar.getChildAt(i) instanceof TextView textView
                    && TextUtils.equals(textView.getText(), titleText)) {
                return textView;
            }
        }
        throw new AssertionError("Toolbar title not found.");
    }

    /** Expected wide display padding for a container: half of the width beyond the content. */
    private static int expectedPaddingDp(int containerWidthDp) {
        return (containerWidthDp - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2;
    }

    private static int dpToPx(int dp) {
        return ViewUtils.dpToPx(
                RuntimeEnvironment.getApplication().getResources().getDisplayMetrics(), dp);
    }
}
