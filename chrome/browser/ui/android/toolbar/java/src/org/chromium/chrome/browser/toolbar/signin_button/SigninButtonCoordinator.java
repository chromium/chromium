// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import android.content.Context;
import android.view.ViewStub;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator for a signin button on the NTP toolbar. Owns the the SigninButton view. */
@NullMarked
public final class SigninButtonCoordinator {
    private final SigninButtonMediator mMediator;
    private final PropertyModel mModel;
    private final @Nullable ViewStub mViewStub;
    private @Nullable SigninButtonView mView;
    private @Nullable PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    public SigninButtonCoordinator(
            Context context,
            ViewStub viewStub,
            MonotonicObservableSupplier<Profile> profileSupplier) {
        mModel = new PropertyModel.Builder(SigninButtonProperties.ALL_KEYS).build();
        mMediator = new SigninButtonMediator(context, mModel, profileSupplier);

        // Defers setting the view and binding the model until the button needs to be shown.
        mViewStub = viewStub;
    }

    /** Updates the button visibility and inflates SigninButton once it should be visible. */
    public void updateButtonVisibility(Boolean showButton) {
        mMediator.updateButtonVisibility(showButton);
        if (mModel.get(SigninButtonProperties.SHOW_BUTTON) && mView == null && mViewStub != null) {

            // Once the view initially is set to be visible, SigninButtonView should be inflated.
            mView = (SigninButtonView) mViewStub.inflate();
            mPropertyModelChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            mModel, mView, SigninButtonViewBinder::bind);
        }
    }

    /** Call to tear down dependencies. */
    public void destroy() {
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
            mPropertyModelChangeProcessor = null;
        }
        mMediator.destroy();
    }
}
