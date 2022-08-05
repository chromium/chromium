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
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

class FastCheckoutCoordinator implements FastCheckoutComponent {
    private FastCheckoutMediator mMediator = new FastCheckoutMediator();

    @Override
    public void initialize(Context context, BottomSheetController sheetController,
            FastCheckoutComponent.Delegate delegate) {
        PropertyModel model = FastCheckoutModel.createDefaultModel(mMediator::onDismissed);
        mMediator.initialize(delegate, model, sheetController);

        LinearLayout rootView = new LinearLayout(context);
        rootView.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        rootView.setOrientation(LinearLayout.VERTICAL);

        // TODO(crbug.com/1334642): Create views for all 3 screens.

        setUpModelChangeProcessors(model, new FastCheckoutView(rootView, sheetController));
    }

    @Override
    public void showOptions(
            FastCheckoutAutofillProfile[] profiles, FastCheckoutCreditCard[] creditCards) {
        mMediator.showOptions(profiles, creditCards);
    }

    static void setUpModelChangeProcessors(PropertyModel model, FastCheckoutView view) {
        PropertyModelChangeProcessor.create(
                model, view, FastCheckoutViewBinder::bindFastCheckoutView);
    }
}
