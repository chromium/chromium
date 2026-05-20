// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import android.util.Pair;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/** Coordinator for the AtMemory Flyout. */
@NullMarked
class AtMemoryFlyoutCoordinator {
    private final AtMemoryFlyoutContent mContent;
    private final AtMemoryFlyoutMediator mMediator;
    private final BottomSheetController mBottomSheetController;

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

    /** Delegate to receive events from the flyout. */
    interface Delegate {
        /** Called when the bottom sheeet is dismissed. */
        void onDismissed();

        /** Called when the back button is clicked. */
        void onBackClicked();

        /** Called when the source button is clicked. */
        void onSourceClicked();

        /** Called when the manage button is clicked. */
        void onManageClicked();

        /**
         * Called when a suggestion chip is clicked.
         *
         * @param text The text label of the clicked chip.
         */
        void onChipClicked(String text);
    }

    AtMemoryFlyoutCoordinator(
            Context context, BottomSheetController sheetController, Delegate delegate) {
        mBottomSheetController = sheetController;

        PropertyModel model =
                new PropertyModel.Builder(AtMemoryFlyoutProperties.ALL_KEYS)
                        .with(AtMemoryFlyoutProperties.ON_BACK_CLICKED, delegate::onBackClicked)
                        .with(AtMemoryFlyoutProperties.ON_SOURCE_CLICKED, delegate::onSourceClicked)
                        .with(AtMemoryFlyoutProperties.ON_MANAGE_CLICKED, delegate::onManageClicked)
                        .with(AtMemoryFlyoutProperties.ON_CHIP_CLICKED, delegate::onChipClicked)
                        .build();

        mMediator = new AtMemoryFlyoutMediator(delegate, model);

        AtMemoryFlyoutView view = new AtMemoryFlyoutView(context);

        mContent = new AtMemoryFlyoutContent(view.getContentView());

        PropertyModelChangeProcessor.create(model, view, AtMemoryFlyoutViewBinder::bind);
    }

    // TODO(crbug.com/505255929): Handle and display chips data.
    void show(List<Pair<String, String>> chipsData) {
        mMediator.setChipsData(chipsData);
        if (mBottomSheetController.requestShowContent(mContent, /* animate= */ true)) {
            mBottomSheetController.addObserver(mBottomSheetObserver);
        } else {
            onDismissed();
        }
    }

    void hide() {
        mBottomSheetController.hideContent(mContent, /* animate= */ true);
    }

    private void onDismissed() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mMediator.onDismissed();
    }

    View getViewForTesting() {
        return mContent.getContentView();
    }
}
