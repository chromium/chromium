// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.HashMap;
import java.util.Map;

/** Tests for {@link HeaderDecoration}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HeaderDecorationUnitTest {
    private static class TestRecyclerView extends RecyclerView {
        private final Map<View, ViewHolder> mViewHolders = new HashMap<>();

        TestRecyclerView(Context context) {
            super(context);
        }

        void registerViewHolder(View view, ViewHolder holder) {
            mViewHolders.put(view, holder);
        }

        @Override
        public ViewHolder getChildViewHolder(View child) {
            return mViewHolders.get(child);
        }
    }

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private RecyclerView.State mState;
    @Mock private Canvas mCanvas;

    private TestRecyclerView mRecyclerView;
    private View mChildViewWithHeader;
    private View mChildViewWithNoHeader;

    private SimpleRecyclerViewAdapter.ViewHolder mShowHeaderViewHolder;
    private SimpleRecyclerViewAdapter.ViewHolder mNoHeaderViewHolder;

    private static final String HEADER_TEXT = "Test Header";

    private PropertyModel mShowHeaderModel;
    private PropertyModel mNoHeaderModel;

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private HeaderDecoration mDecoration;
    private int mExpectedHeight;

    @Before
    public void setUp() {
        mShowHeaderModel =
                new PropertyModel.Builder(SuggestionCommonProperties.ALL_KEYS)
                        .with(SuggestionCommonProperties.HEADER_TITLE, HEADER_TEXT)
                        .with(
                                SuggestionCommonProperties.COLOR_SCHEME,
                                BrandedColorScheme.APP_DEFAULT)
                        .build();
        mNoHeaderModel =
                new PropertyModel.Builder(SuggestionCommonProperties.ALL_KEYS)
                        .with(SuggestionCommonProperties.HEADER_TITLE, null)
                        .build();

        mActivityController = Robolectric.buildActivity(Activity.class);
        mActivity = mActivityController.setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mDecoration = new HeaderDecoration(mActivity);
        mRecyclerView = new TestRecyclerView(mActivity);
        mChildViewWithHeader = new View(mActivity);
        mChildViewWithNoHeader = new View(mActivity);

        mExpectedHeight =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_header_height);

        mShowHeaderViewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(mChildViewWithHeader, null);
        mNoHeaderViewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(mChildViewWithNoHeader, null);
        mShowHeaderViewHolder.model = mShowHeaderModel;
        mNoHeaderViewHolder.model = mNoHeaderModel;

        mRecyclerView.registerViewHolder(mChildViewWithHeader, mShowHeaderViewHolder);
        mRecyclerView.registerViewHolder(mChildViewWithNoHeader, mNoHeaderViewHolder);
        mRecyclerView.addView(mChildViewWithHeader);
        mRecyclerView.addView(mChildViewWithNoHeader);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    @Test
    public void testGetItemOffsets_withHeader() {
        Rect outRect = new Rect();
        mDecoration.getItemOffsets(outRect, mChildViewWithHeader, mRecyclerView, mState);
        assertEquals(mExpectedHeight, outRect.top);
        assertEquals(0, outRect.bottom);
        assertEquals(0, outRect.left);
        assertEquals(0, outRect.right);
    }

    @Test
    public void testGetItemOffsets_noHeader() {
        Rect outRect = new Rect();
        mDecoration.getItemOffsets(outRect, mChildViewWithNoHeader, mRecyclerView, mState);
        assertEquals(0, outRect.top);
        assertEquals(0, outRect.bottom);
        assertEquals(0, outRect.left);
        assertEquals(0, outRect.right);
    }

    @Test
    public void testDraw_withHeader() {
        mChildViewWithHeader.setTop(0);
        mChildViewWithHeader.setTranslationY(0.0f);
        mRecyclerView.setRight(100);
        mRecyclerView.setPadding(0, 0, 0, 0);
        mRecyclerView.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);
        verify(mCanvas).drawText(eq(HEADER_TEXT), anyFloat(), anyFloat(), any(Paint.class));
    }

    @Test
    public void testDraw_noHeader() {
        mRecyclerView.removeView(mChildViewWithHeader);

        mDecoration.onDraw(mCanvas, mRecyclerView, mState);
        verifyNoInteractions(mCanvas);
    }
}
