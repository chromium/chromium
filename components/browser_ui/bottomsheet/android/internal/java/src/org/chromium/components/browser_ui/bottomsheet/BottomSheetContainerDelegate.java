// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewParent;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.List;

/**
 * Applies container margin and clipping updates to the bottom sheet's host container and its
 * ancestor views up to {@code android.R.id.content}.
 */
@NullMarked
class BottomSheetContainerDelegate implements BottomSheetMediator.SheetContainerDelegate {
    private static final class SavedAncestorClipState {
        final ViewGroup mView;
        final boolean mClipChildren;
        final boolean mClipToPadding;

        SavedAncestorClipState(ViewGroup view, boolean clipChildren, boolean clipToPadding) {
            mView = view;
            mClipChildren = clipChildren;
            mClipToPadding = clipToPadding;
        }
    }

    private final List<SavedAncestorClipState> mSavedAncestorClipStates = new ArrayList<>();
    private ViewGroup mSheetContainer;
    private boolean mAncestorsUnclipped;

    BottomSheetContainerDelegate(ViewGroup sheetContainer) {
        mSheetContainer = sheetContainer;
    }

    @Override
    public void setContainerBottomMargin(@Px int bottomMargin) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) mSheetContainer.getLayoutParams();
        layoutParams.bottomMargin = bottomMargin;
        mSheetContainer.setLayoutParams(layoutParams);
    }

    @Override
    public void setContainerClipToPadding(boolean clipToPadding) {
        // Container child clipping must always remain false so both mobile top-shadow
        // translations and popup glow can render past bounds without being sliced.
        mSheetContainer.setClipChildren(false);
        mSheetContainer.setClipToPadding(clipToPadding);
    }

    @Override
    public void setAncestorsUnclipped(boolean unclipped) {
        if (mAncestorsUnclipped == unclipped) return;
        mAncestorsUnclipped = unclipped;
        if (unclipped) {
            for (ViewParent parent = mSheetContainer.getParent();
                    parent instanceof ViewGroup ancestor;
                    parent = ancestor.getParent()) {
                boolean clipChildren = ancestor.getClipChildren();
                boolean clipToPadding = ancestor.getClipToPadding();
                if (clipChildren || clipToPadding) {
                    mSavedAncestorClipStates.add(
                            new SavedAncestorClipState(ancestor, clipChildren, clipToPadding));
                    ancestor.setClipChildren(false);
                    ancestor.setClipToPadding(false);
                }
                if (ancestor.getId() == android.R.id.content) break;
            }
        } else {
            for (int i = 0; i < mSavedAncestorClipStates.size(); i++) {
                SavedAncestorClipState state = mSavedAncestorClipStates.get(i);
                state.mView.setClipChildren(state.mClipChildren);
                state.mView.setClipToPadding(state.mClipToPadding);
            }
            mSavedAncestorClipStates.clear();
        }
    }

    void setSheetContainerForTesting(ViewGroup sheetContainer) {
        if (mSheetContainer == sheetContainer) return;
        setAncestorsUnclipped(false);
        mSheetContainer = sheetContainer;
    }
}
