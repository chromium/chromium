// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.signin.account_picker;

import android.graphics.Rect;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.ui.base.ViewUtils;

/** Item decoration which updates the background and the margins of the account list items. */
public final class AccountPickerItemDecoration extends RecyclerView.ItemDecoration {
    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        super.getItemOffsets(outRect, view, parent, state);

        int position = parent.getChildAdapterPosition(view);
        int itemsCount = parent.getAdapter().getItemCount();

        @DrawableRes int backgroundId = getBackgroundDrawableId(position, itemsCount);
        view.setBackgroundResource(backgroundId);
        if (position != itemsCount - 1) {
            outRect.bottom = ViewUtils.dpToPx(view.getContext(), 2);
        }
    }

    private int getBackgroundDrawableId(int position, int itemCount) {
        if (itemCount == 1) { // Round all edges of the only item.
            return R.drawable.account_row_background_rounded_all;
        }
        if (position == itemCount - 1) { // Round the bottom of the last item.
            return R.drawable.account_row_background_rounded_down;
        }
        if (position == 0) { // Round the top of the first item.
            return R.drawable.account_row_background_rounded_up;
        }
        // The rest of the items have a background with no rounded edges.
        return R.drawable.account_row_background;
    }
}
