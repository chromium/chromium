// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.build.NullUtil.assertNonNull;

import android.view.KeyEvent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.suggestions.SelectionController;

import java.util.ArrayList;
import java.util.List;

/**
 * SelectionController that handles selection of the views in the LocationBar. It supports skipping
 * of views that are not visible and allows key handling code to know if the autocomplete list is
 * currently selected.
 */
@NullMarked
public class LocationBarSelectionController extends SelectionController {

    private final List<SelectableView> mSelectableViews;
    private final List<SelectableView> mVisibleViewsHolder = new ArrayList<>();
    private @Nullable Runnable mOnSelectionChanged;

    public interface SelectableView {
        default boolean isAutocompleteList() {
            return false;
        }

        boolean isVisible();

        void setSelected(boolean isSelected);

        void handleActivationEvent(KeyEvent event);
    }

    /**
     * @param selectableViews The list of views that this controller addresses/
     */
    public LocationBarSelectionController(List<SelectableView> selectableViews) {
        super(TraversalMode.SATURATING);
        mSelectableViews = selectableViews;
        reset();
    }

    /**
     * Sets the callback invoked whenever selection changes, and invokes it immediately to sync
     * initial state.
     *
     * <p>Not passed in the constructor to avoid invoking callbacks before callers finish assigning
     * this instance to their member fields.
     *
     * @param onSelectionChanged The callback to invoke when selection changes.
     */
    public void setSelectionChangedCallback(Runnable onSelectionChanged) {
        mOnSelectionChanged = onSelectionChanged;
        mOnSelectionChanged.run();
    }

    public SelectableView getSelectedView() {
        Integer position = getPosition();
        assertNonNull(position);
        List<SelectableView> visibleViews = getVisibleViews();
        position = Math.min(position, visibleViews.size() - 1);

        return visibleViews.get(position);
    }

    public boolean selectAutocompleteList() {
        List<SelectableView> visibleViews = getVisibleViews();
        for (int i = 0; i < visibleViews.size(); i++) {
            if (visibleViews.get(i).isAutocompleteList()) {
                return setPosition(i);
            }
        }
        return false;
    }

    @Override
    public void reset() {
        for (int i = 0; i < mSelectableViews.size(); i++) {
            mSelectableViews.get(i).setSelected(false);
        }
        super.reset();
    }

    boolean isAutocompleteListSelected() {
        return getSelectedView().isAutocompleteList();
    }

    @Override
    protected int getItemCount() {
        return getVisibleViews().size();
    }

    @Override
    protected boolean setPosition(int newPosition) {
        boolean result = super.setPosition(newPosition);
        if (result && mOnSelectionChanged != null) {
            mOnSelectionChanged.run();
        }
        return result;
    }

    @Override
    protected void setItemState(int position, boolean isSelected) {
        List<SelectableView> visibleViews = getVisibleViews();
        if (position >= visibleViews.size()) return;
        visibleViews.get(position).setSelected(isSelected);
    }

    private List<SelectableView> getVisibleViews() {
        mVisibleViewsHolder.clear();
        for (int i = 0; i < mSelectableViews.size(); i++) {
            SelectableView view = mSelectableViews.get(i);
            if (view.isVisible()) {
                mVisibleViewsHolder.add(view);
            }
        }

        return mVisibleViewsHolder;
    }

    /* package */ @TraversalMode
    int getSelectionModeForTesting() {
        return mMode;
    }
}
