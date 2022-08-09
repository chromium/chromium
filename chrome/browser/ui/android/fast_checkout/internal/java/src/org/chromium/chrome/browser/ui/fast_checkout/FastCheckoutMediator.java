// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import android.view.View;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.components.autofill_assistant.AutofillAssistantPublicTags;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the logic for the FastCheckout component. It sets the state of the model and reacts
 * to events like clicks.
 */
public class FastCheckoutMediator {
    private PropertyModel mModel;
    private FastCheckoutComponent.Delegate mDelegate;
    private BottomSheetController mBottomSheetController;
    private BottomSheetObserver mBottomSheetClosedObserver;
    private BottomSheetObserver mBottomSheetDismissedObserver;

    void initialize(FastCheckoutComponent.Delegate delegate, PropertyModel model,
            BottomSheetController bottomSheetController) {
        mModel = model;
        mDelegate = delegate;
        mBottomSheetController = bottomSheetController;

        mBottomSheetClosedObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
                if (newContent == null) {
                    mModel.set(FastCheckoutModel.VISIBLE, true);
                    mBottomSheetController.removeObserver(mBottomSheetClosedObserver);
                }
            }
        };

        mBottomSheetDismissedObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                super.onSheetClosed(reason);
                dismiss(reason);
                mBottomSheetController.removeObserver(mBottomSheetDismissedObserver);
            }
        };
    }

    public void showOptions(
            FastCheckoutAutofillProfile[] profiles, FastCheckoutCreditCard[] creditCards) {
        // TODO(crbug.com/1334642): Add |profiles| and |creditCards| to the model.

        // It is possible that FC onboarding has been just accepted but the bottom sheet is still
        // showing. If that's the case we try hiding it and then show FC bottom sheet.
        if (isOnboardingSheet()) {
            // Delay showing the bottom sheet until the consent sheet is fully closed.
            mBottomSheetController.addObserver(mBottomSheetClosedObserver);
            mBottomSheetController.hideContent(
                    mBottomSheetController.getCurrentSheetContent(), /* animate= */ true);
        } else {
            mModel.set(FastCheckoutModel.VISIBLE, true);
        }
    }

    /**
     * If set to true, requests to show the bottom sheet. Otherwise, requests to hide the sheet.
     * @param isVisible A boolean describing whether to show or hide the sheet.
     * @param content The bottom sheet content to show/hide.
     * @return True if the request was successful, false otherwise.
     */
    public boolean setVisible(boolean isVisible, BottomSheetContent content) {
        if (isVisible) {
            mBottomSheetController.addObserver(mBottomSheetDismissedObserver);
            if (!mBottomSheetController.requestShowContent(content, true)) {
                mBottomSheetController.removeObserver(mBottomSheetDismissedObserver);
                return false;
            }
        } else {
            mBottomSheetController.hideContent(content, true);
        }
        return true;
    }

    /**
     * Dismisses the current bottom sheet.
     */
    public void dismiss(@StateChangeReason int reason) {
        if (!mModel.get(FastCheckoutModel.VISIBLE)) return; // Dismiss only if not dismissed yet.
        // TODO(crbug.com/1334642): Record dismissal metrics.
        mModel.set(FastCheckoutModel.VISIBLE, false);
        mDelegate.onDismissed();
    }

    private boolean isOnboardingSheet() {
        if (mBottomSheetController == null
                || mBottomSheetController.getCurrentSheetContent() == null) {
            return false;
        }

        View view = mBottomSheetController.getCurrentSheetContent().getContentView();
        return view.getTag() != null
                && view.getTag().equals(
                        AutofillAssistantPublicTags.AUTOFILL_ASSISTANT_BOTTOM_SHEET_CONTENT_TAG);
    }

    /**
     * Releases the resources used by FastCheckoutMediator.
     */
    @MainThread
    private void destroy() {
        mBottomSheetController.removeObserver(mBottomSheetDismissedObserver);
    }
}
