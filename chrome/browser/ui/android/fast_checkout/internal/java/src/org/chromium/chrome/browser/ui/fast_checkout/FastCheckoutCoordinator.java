// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ViewFlipper;

import org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.chrome.browser.ui.fast_checkout.detail_screen.DetailScreenCoordinator;
import org.chromium.chrome.browser.ui.fast_checkout.home_screen.HomeScreenCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

class FastCheckoutCoordinator implements FastCheckoutComponent {
    private FastCheckoutMediator mMediator = new FastCheckoutMediator();
    private PropertyModel mModel = FastCheckoutProperties.createDefaultModel();
    private FastCheckoutSheetContent mContent;
    private BottomSheetController mBottomSheetController;

    @Override
    public void initialize(
            Context context,
            BottomSheetController sheetController,
            FastCheckoutComponent.Delegate delegate) {
        mBottomSheetController = sheetController;
        mMediator.initialize(delegate, mModel, mBottomSheetController);

        LinearLayout rootView =
                (LinearLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.fast_checkout_bottom_sheet, null);
        mContent = new FastCheckoutSheetContent(mMediator, rootView);

        View homeScreenView = rootView.findViewById(R.id.fast_checkout_home_screen_sheet);
        new HomeScreenCoordinator(context, homeScreenView, mModel);

        // The detail screen can display the Autofill profile or the credit
        // card selection.
        View detailScreenView = rootView.findViewById(R.id.fast_checkout_detail_screen_sheet);
        new DetailScreenCoordinator(context, detailScreenView, mModel, mBottomSheetController);

        ViewFlipper viewFlipperView =
                (ViewFlipper) rootView.findViewById(R.id.fast_checkout_bottom_sheet_view_flipper);
        mModel.addObserver(
                (source, propertyKey) -> {
                    if (FastCheckoutProperties.CURRENT_SCREEN == propertyKey) {
                        viewFlipperView.setDisplayedChild(
                                getScreenIndexForScreenType(
                                        mModel.get(FastCheckoutProperties.CURRENT_SCREEN)));
                    } else if (FastCheckoutProperties.VISIBLE == propertyKey) {
                        // Dismiss the sheet if it can't be immediately shown.
                        boolean visibilityChangeSuccessful =
                                mMediator.setVisible(
                                        mModel.get(FastCheckoutProperties.VISIBLE), mContent);
                        if (!visibilityChangeSuccessful
                                && mModel.get(FastCheckoutProperties.VISIBLE)) {
                            mMediator.dismiss(BottomSheetController.StateChangeReason.NONE);
                        }
                    }
                });
    }

    /**
     * Acts as a helper function to convert a {@link FastCheckoutProperties.ScreenType}
     * the index of the screen in the ViewFlipper.
     */
    private static int getScreenIndexForScreenType(@ScreenType int screenType) {
        switch (screenType) {
            case ScreenType.HOME_SCREEN:
                return 0;
                // Both the Autofill profile selection and the credit card selection
                // are displayed on the detail screen.
            case ScreenType.AUTOFILL_PROFILE_SCREEN:
            case ScreenType.CREDIT_CARD_SCREEN:
                return 1;
        }
        assert false : "Undefined ScreenType: " + screenType;
        return 0;
    }

    @Override
    public void showOptions(
            List<FastCheckoutAutofillProfile> profiles, List<FastCheckoutCreditCard> creditCards) {
        mMediator.showOptions(profiles, creditCards);
    }

    @Override
    public void destroy() {
        mMediator.destroy();
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }
}
