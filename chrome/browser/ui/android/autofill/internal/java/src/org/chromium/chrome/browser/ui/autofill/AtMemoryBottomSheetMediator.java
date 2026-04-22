// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Contains the business logic for the AtMemoryBottomSheet. */
@NullMarked
class AtMemoryBottomSheetMediator {
    private @Nullable PropertyModel mModel;
    private AtMemoryBottomSheetCoordinator.@Nullable Delegate mDelegate;
    private @Nullable BottomSheetController mBottomSheetController;
    private @Nullable AtMemoryBottomSheetContent mContent;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                    super.onSheetClosed(reason);
                    onDismissed();
                    if (mBottomSheetController != null) {
                        mBottomSheetController.removeObserver(this);
                    }
                }
            };

    void initialize(
            AtMemoryBottomSheetCoordinator.Delegate delegate,
            PropertyModel model,
            BottomSheetController bottomSheetController,
            AtMemoryBottomSheetContent content) {
        mDelegate = delegate;
        mModel = model;
        mBottomSheetController = bottomSheetController;
        mContent = content;
    }

    void show() {
        if (mBottomSheetController != null && mContent != null) {
            mBottomSheetController.addObserver(mBottomSheetObserver);
            if (!mBottomSheetController.requestShowContent(mContent, true)) {
                mBottomSheetController.removeObserver(mBottomSheetObserver);
                onDismissed();
            } else if (mModel != null) {
                mModel.set(AtMemoryBottomSheetProperties.VISIBLE, true);
            }
        }
    }

    void onDismissed() {
        if (mModel != null) {
            mModel.set(AtMemoryBottomSheetProperties.VISIBLE, false);
        }
        if (mDelegate != null) {
            mDelegate.onDismissed();
        }
    }

    void destroy() {
        if (mModel != null) {
            mModel.set(AtMemoryBottomSheetProperties.VISIBLE, false);
        }
        if (mBottomSheetController != null && mContent != null) {
            mBottomSheetController.hideContent(mContent, true);
            mBottomSheetController.removeObserver(mBottomSheetObserver);
        }
    }
}
