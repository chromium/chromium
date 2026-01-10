// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.Context;
import android.util.AttributeSet;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.base.SpacingRecyclerViewItemDecoration;

/** A RecyclerView for the FuseboxAttachment component. */
@NullMarked
public class FuseboxAttachmentRecyclerView extends RecyclerView {
    @VisibleForTesting
    /**
     * An {@link AdapterDataObserver} that scrolls the {@link RecyclerView} to the end whenever new
     * items are inserted. This ensures that newly added Fusebox attachments are always visible to
     * the user.
     *
     * @see FuseboxAttachmentRecyclerView#setAdapter(Adapter)
     */
    /* package */ static class ScrollToEndOnInsertionObserver extends AdapterDataObserver {
        private final RecyclerView mView;

        /**
         * Creates a new ScrollToEndOnInsertionObserver.
         *
         * @param view The {@link RecyclerView} to act on.
         */
        @VisibleForTesting
        /* package */ ScrollToEndOnInsertionObserver(RecyclerView view) {
            mView = view;
        }

        @Override
        public void onItemRangeInserted(int startPosition, int itemCount) {
            super.onItemRangeInserted(startPosition, itemCount);
            mView.scrollToPosition(startPosition + itemCount - 1);
        }
    }

    private final ScrollToEndOnInsertionObserver mScrollToEndOnInsertion;

    public FuseboxAttachmentRecyclerView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mScrollToEndOnInsertion = new ScrollToEndOnInsertionObserver(this);
        setLayoutManager(new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false));
        addItemDecoration(
                new SpacingRecyclerViewItemDecoration(
                        /* leadInSpace= */ 0,
                        /* elementSpace= */ context.getResources()
                                .getDimensionPixelSize(R.dimen.omnibox_action_chip_spacing)));
    }

    @Override
    public void setAdapter(@Nullable Adapter newAdapter) {
        var oldAdapter = getAdapter();
        if (oldAdapter != null) {
            oldAdapter.unregisterAdapterDataObserver(mScrollToEndOnInsertion);
        }
        super.setAdapter(newAdapter);
        if (newAdapter != null) {
            newAdapter.registerAdapterDataObserver(mScrollToEndOnInsertion);
        }
    }
}
