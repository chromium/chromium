// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.home_screen;

import android.content.Context;
import android.view.View;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the home screen of the Fast Checkout bottom sheet. */
public class HomeScreenCoordinator {
    /** The delegate of the class. */
    public interface Delegate {
        /** The current selected options were accepted. */
        void onOptionsAccepted();

        /** The user clicked on the selected address item. */
        void onShowAddressesList();

        /** The user clicked on the selected credit card item. */
        void onShowCreditCardList();
    }

    public HomeScreenCoordinator(Context context, View view, PropertyModel model) {
        // Bind view and mediator through the model.
        HomeScreenViewBinder.ViewHolder viewHolder =
                new HomeScreenViewBinder.ViewHolder(context, view);

        PropertyModelChangeProcessor.create(model, viewHolder, HomeScreenViewBinder::bind);
    }
}
