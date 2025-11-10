// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;

/** A RecyclerView for the FuseboxAttachment component. */
@NullMarked
public class FuseboxAttachmentRecyclerView extends RecyclerView {
    private static class SpacingItemDecoration extends RecyclerView.ItemDecoration {
        private final int mSpacing;

        public SpacingItemDecoration(Context context) {
            mSpacing =
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.omnibox_action_chip_spacing);
        }

        @Override
        public void getItemOffsets(
                Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
            if (parent.getChildAdapterPosition(view) != 0) {
                outRect.left = mSpacing;
            }
        }
    }

    public FuseboxAttachmentRecyclerView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutManager(new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false));
        addItemDecoration(new SpacingItemDecoration(context));
    }
}
