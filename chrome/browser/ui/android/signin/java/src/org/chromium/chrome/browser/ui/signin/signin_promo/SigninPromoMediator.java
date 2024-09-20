// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.signin_promo;

import android.content.Context;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.ui.modelutil.PropertyModel;

final class SigninPromoMediator implements ProfileDataCache.Observer {
    private final PropertyModel mModel;
    private final ProfileDataCache mProfileDataCache;

    SigninPromoMediator(
            Context context,
            @StringRes int titleStringId,
            @StringRes int descriptionStringId,
            boolean shouldSuppressSecondaryButton,
            boolean shouldHideDismissButton) {
        mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(context);
        // TODO(crbug.com/327387704): Fetch the account email and use it to update the
        // ProfileDataCache.
        mModel =
                SigninPromoProperties.createModel(
                        mProfileDataCache.getProfileDataOrDefault(""),
                        this::onAcceptClicked,
                        this::onDeclineClicked,
                        titleStringId,
                        descriptionStringId,
                        shouldSuppressSecondaryButton,
                        shouldHideDismissButton);
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        mModel.set(
                SigninPromoProperties.PROFILE_DATA,
                mProfileDataCache.getProfileDataOrDefault(accountEmail));
    }

    PropertyModel getModel() {
        return mModel;
    }

    private void onAcceptClicked(View view) {
        // TODO(crbug.com/327387704): Implement this method
    }

    private void onDeclineClicked(View view) {
        // TODO(crbug.com/327387704): Implement this method
    }
}
