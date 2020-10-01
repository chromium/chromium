// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.list;

import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;
import androidx.recyclerview.widget.RecyclerView.State;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 *  The top level coordinator for the video tutorials list UI.
 */
public class TutorialListCoordinatorImpl implements TutorialListCoordinator {
    private final TutorialListMediator mMediator;

    /**
     * Constructor.
     * @param recyclerView The {@link RecyclerView} associated with this coordinator.
     * @param videoTutorialService The video tutorial service backend.
     * @param imageFetcher An {@link ImageFetcher} to provide thumbnail images.
     * @param clickCallback A callback to be invoked when a tutorial is clicked.
     */
    public TutorialListCoordinatorImpl(RecyclerView recyclerView,
            VideoTutorialService videoTutorialService, ImageFetcher imageFetcher,
            Callback<Tutorial> clickCallback) {
        MVCListAdapter.ModelList listModel = new MVCListAdapter.ModelList();
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(listModel);
        adapter.registerType(TutorialCardProperties.VIDEO_TUTORIAL_CARD_VIEW_TYPE,
                TutorialCardViewBinder::buildView, TutorialCardViewBinder::bindView);
        recyclerView.setAdapter(adapter);
        recyclerView.setLayoutManager(new LinearLayoutManager(
                recyclerView.getContext(), LinearLayoutManager.VERTICAL, false));
        recyclerView.addItemDecoration(new ItemDecorationImpl(recyclerView.getResources()));

        mMediator = new TutorialListMediator(listModel, recyclerView.getContext(),
                videoTutorialService, imageFetcher, clickCallback);
    }

    private class ItemDecorationImpl extends ItemDecoration {
        private final int mInterImagePaddingPx;
        public ItemDecorationImpl(Resources resources) {
            mInterImagePaddingPx = resources.getDimensionPixelOffset(R.dimen.card_padding);
        }

        @Override
        public void getItemOffsets(@NonNull Rect outRect, @NonNull View view,
                @NonNull RecyclerView parent, @NonNull State state) {
            outRect.top = mInterImagePaddingPx / 2;
            outRect.bottom = mInterImagePaddingPx / 2;
        }
    }
}