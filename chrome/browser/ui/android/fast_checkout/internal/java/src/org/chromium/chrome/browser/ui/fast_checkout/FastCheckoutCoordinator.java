// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import android.content.Context;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

class FastCheckoutCoordinator implements FastCheckoutComponent {
    private FastCheckoutMediator mMediator = new FastCheckoutMediator();
    private PropertyModel mModel = FastCheckoutModel.createDefaultModel();
    private FastCheckoutSheetContent mContent;

    @Override
    public void initialize(Context context, BottomSheetController sheetController,
            FastCheckoutComponent.Delegate delegate) {
        mMediator.initialize(delegate, mModel, sheetController);

        LinearLayout rootView = new LinearLayout(context);
        rootView.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        rootView.setOrientation(LinearLayout.VERTICAL);

        mContent = new FastCheckoutSheetContent(rootView);

        // TODO(crbug.com/1334642): Create views for all 3 screens.
        mModel.addObserver((source, propertyKey) -> {
            if (FastCheckoutModel.CURRENT_SCREEN == propertyKey) {
                mContent.updateCurrentScreen(mModel.get(FastCheckoutModel.CURRENT_SCREEN));
            } else if (FastCheckoutModel.VISIBLE == propertyKey) {
                // Dismiss the sheet if it can't be immediately shown.
                boolean visibilityChangeSuccessful =
                        mMediator.setVisible(mModel.get(FastCheckoutModel.VISIBLE), mContent);
                if (!visibilityChangeSuccessful && mModel.get(FastCheckoutModel.VISIBLE)) {
                    mMediator.dismiss(BottomSheetController.StateChangeReason.NONE);
                }
            }
        });
    }

    @Override
    public void showOptions(
            FastCheckoutAutofillProfile[] profiles, FastCheckoutCreditCard[] creditCards) {
        mMediator.showOptions(profiles, creditCards);
    }
}
