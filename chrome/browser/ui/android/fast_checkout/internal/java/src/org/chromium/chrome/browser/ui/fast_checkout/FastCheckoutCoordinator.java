// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.ViewFlipper;

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

        ViewFlipper rootView = (ViewFlipper) LayoutInflater.from(context).inflate(
                R.layout.fast_checkout_bottom_sheet, null);
        mContent = new FastCheckoutSheetContent(rootView);

        mModel.addObserver((source, propertyKey) -> {
            if (FastCheckoutModel.CURRENT_SCREEN == propertyKey) {
                rootView.setDisplayedChild(mModel.get(FastCheckoutModel.CURRENT_SCREEN));
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
