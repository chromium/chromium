// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/** Coordinator for the AtMemoryBottomSheet. */
@NullMarked
public class AtMemoryBottomSheetCoordinator {
    private final AtMemoryBottomSheetContent mContent;
    private final AtMemoryBottomSheetMediator mMediator;
    private final BottomSheetController mBottomSheetController;

    public static final int ITEM_TYPE_SUGGESTION = 1;
    public static final int ITEM_TYPE_SEARCH_TILE = 2;
    public static final int ITEM_TYPE_ZERO_STATE = 3;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                    super.onSheetClosed(reason);
                    if (mBottomSheetController.getCurrentSheetContent() != null
                            && mBottomSheetController.getCurrentSheetContent() == mContent) {
                        onDismissed();
                    }
                }
            };

    /** Delegate to receive events from the bottom sheet. */
    interface Delegate {
        void onDismissed();

        void onQuerySubmitted(String query);

        void onQueryTextChanged(String query);

        void onSearchFocus(boolean hasFocus);

        void onSuggestionClicked(int position);

        void onChildSuggestionsShown(int parentPosition);

        void onChildSuggestionClicked(int parentPosition, int childPosition);

        boolean isSearching();
    }

    AtMemoryBottomSheetCoordinator(
            Context context,
            BottomSheetController sheetController,
            Delegate delegate,
            Profile profile) {
        mBottomSheetController = sheetController;

        AtMemoryBottomSheetView view = new AtMemoryBottomSheetView(context);

        mMediator = new AtMemoryBottomSheetMediator(context, profile, delegate, view);

        mContent = new AtMemoryBottomSheetContent(view, mBottomSheetController);

        setUpModelChangeProcessors(view);
    }

    public void show(List<AutofillSuggestion> suggestions) {
        mBottomSheetController.addObserver(mBottomSheetObserver);
        if (mBottomSheetController.requestShowContent(mContent, /* animate= */ true)) {
            mMediator.show(suggestions);
        } else {
            onDismissed();
        }
    }

    public void hide() {
        mBottomSheetController.hideContent(mContent, /* animate= */ true);
    }

    public void expandSheet() {
        mBottomSheetController.expandSheet(/* animate= */ true);
    }

    private void onDismissed() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mMediator.onDismissed();
    }

    AtMemoryBottomSheetContent getBottomSheetContentForTesting() {
        return mContent;
    }

    /**
     * Sets up the Model Change Processors (MCPs) to bind the separate property models for the
     * bottom sheet, the home screen, and the flyout screen to their respective views.
     */
    private void setUpModelChangeProcessors(AtMemoryBottomSheetView view) {
        PropertyModelChangeProcessor.create(
                mMediator.getModel(),
                view,
                AtMemoryBottomSheetViewBinder::bindAtMemoryBottomSheetView);

        PropertyModelChangeProcessor.create(
                mMediator.getHomeModel(),
                view.getHomeView(),
                AtMemoryBottomSheetViewBinder::bindAtMemoryHomeView);

        PropertyModelChangeProcessor.create(
                mMediator.getFlyoutModel(),
                view.getFlyoutView(),
                AtMemoryBottomSheetViewBinder::bindAtMemoryFlyoutView);
    }
}
