// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.list;

import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;
import androidx.recyclerview.widget.RecyclerView.State;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;
import org.chromium.components.browser_ui.widget.FadingShadow;
import org.chromium.components.browser_ui.widget.FadingShadowView;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 *  The top level coordinator for the video tutorials list UI.
 */
public class TutorialListCoordinatorImpl implements TutorialListCoordinator {
    private final TutorialListMediator mMediator;
    private final ViewGroup mMainView;
    private UiConfig mUiConfig;

    /**
     * Constructor.
     * @param mainView The {@link View} associated with this coordinator.
     * @param videoTutorialService The video tutorial service backend.
     * @param imageFetcher An {@link ImageFetcher} to provide thumbnail images.
     * @param clickCallback A callback to be invoked when a tutorial is clicked.
     */
    public TutorialListCoordinatorImpl(ViewGroup mainView,
            VideoTutorialService videoTutorialService, ImageFetcher imageFetcher,
            Callback<Tutorial> clickCallback) {
        mMainView = mainView;
        MVCListAdapter.ModelList listModel = new MVCListAdapter.ModelList();
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(listModel);
        adapter.registerType(TutorialCardProperties.VIDEO_TUTORIAL_CARD_VIEW_TYPE,
                TutorialCardViewBinder::buildView, TutorialCardViewBinder::bindView);
        final RecyclerView recyclerView = mainView.findViewById(R.id.recycler_view);
        recyclerView.setAdapter(adapter);
        recyclerView.setLayoutManager(new LinearLayoutManager(
                recyclerView.getContext(), LinearLayoutManager.VERTICAL, false));
        recyclerView.addItemDecoration(new ItemDecorationImpl(recyclerView.getResources()));

        mMediator = new TutorialListMediator(listModel, recyclerView.getContext(),
                videoTutorialService, imageFetcher, clickCallback);

        FadingShadowView toolbarShadow = mainView.findViewById(R.id.toolbar_shadow);
        toolbarShadow.init(toolbarShadow.getContext().getColor(R.color.toolbar_shadow_color),
                FadingShadow.POSITION_TOP);

        recyclerView.addOnScrollListener(new OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                boolean showShadow = recyclerView.canScrollVertically(-1);
                toolbarShadow.setVisibility(showShadow ? View.VISIBLE : View.GONE);
            }
        });

        configureWideDisplayStyle();
    }

    private void configureWideDisplayStyle() {
        mUiConfig = new UiConfig(mMainView);
        mUiConfig.addObserver(newDisplayStyle -> {
            int padding = getPaddingForDisplayStyle(newDisplayStyle, mMainView.getResources());
            View recyclerView = mMainView.findViewById(R.id.recycler_view);
            View toolbar = mMainView.findViewById(R.id.toolbar);
            ViewCompat.setPaddingRelative(recyclerView, padding, recyclerView.getPaddingTop(),
                    padding, recyclerView.getPaddingBottom());
            ViewCompat.setPaddingRelative(
                    toolbar, padding, toolbar.getPaddingTop(), padding, toolbar.getPaddingBottom());
        });

        mMainView.addView(new View(mMainView.getContext()) {
            @Override
            protected void onConfigurationChanged(Configuration newConfig) {
                mUiConfig.updateDisplayStyle();
            }
        });
    }

    private static int getPaddingForDisplayStyle(DisplayStyle displayStyle, Resources resources) {
        int padding = 0;
        if (displayStyle.horizontal == HorizontalDisplayStyle.WIDE) {
            int screenWidthDp = resources.getConfiguration().screenWidthDp;
            padding = (int) (((screenWidthDp - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2.f)
                    * resources.getDisplayMetrics().density);
            padding =
                    (int) Math.max(resources.getDimensionPixelSize(R.dimen.card_padding), padding);
        }
        return padding;
    }

    private class ItemDecorationImpl extends ItemDecoration {
        private final int mVerticalInterCardPaddingPx;
        private final int mHorizontalStartPaddingPx;

        public ItemDecorationImpl(Resources resources) {
            mVerticalInterCardPaddingPx = resources.getDimensionPixelOffset(R.dimen.card_padding);
            mHorizontalStartPaddingPx = resources.getDimensionPixelOffset(R.dimen.card_padding);
        }

        @Override
        public void getItemOffsets(@NonNull Rect outRect, @NonNull View view,
                @NonNull RecyclerView parent, @NonNull State state) {
            outRect.top = mVerticalInterCardPaddingPx / 2;
            outRect.bottom = mVerticalInterCardPaddingPx / 2;
            outRect.left = mHorizontalStartPaddingPx;
            outRect.right = mHorizontalStartPaddingPx;
        }
    }
}