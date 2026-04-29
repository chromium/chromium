// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import android.content.Context;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

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
        void onDismissed();
    }

    AtMemoryFlyoutCoordinator(
            Context context, BottomSheetController sheetController, Delegate delegate) {
        mBottomSheetController = sheetController;

        PropertyModel model =
                new PropertyModel.Builder(AtMemoryFlyoutProperties.ALL_KEYS)
                        .build();

        mMediator = new AtMemoryFlyoutMediator(delegate, model);

        mContent = new AtMemoryFlyoutContent(context);

        PropertyModelChangeProcessor.create(
                        model, mContent.getContentView(), AtMemoryFlyoutViewBinder::bind);

    }

    void show() {
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
}
