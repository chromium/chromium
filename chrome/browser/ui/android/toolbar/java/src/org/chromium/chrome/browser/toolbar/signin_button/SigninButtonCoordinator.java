// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import android.content.Context;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator for a signin button on the NTP toolbar. Owns the the SigninButton view.
 * TODO(crbug.com/475816843): Ensure implementation is complete and the View is inflated.
 */
@NullMarked
public final class SigninButtonCoordinator {
    private final SigninButtonMediator mMediator;
    private final SigninButtonView mView;
    private @Nullable PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    public SigninButtonCoordinator(
            Context context, MonotonicObservableSupplier<Profile> profileSupplier) {
        PropertyModel model = new PropertyModel.Builder(SigninButtonProperties.ALL_KEYS).build();
        mMediator = new SigninButtonMediator(profileSupplier);
        mView = new SigninButtonView(context);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(model, mView, SigninButtonViewBinder::bind);
    }

    public @Nullable SigninButtonView getView() {
        return mView;
    }

    public void destroy() {
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
            mPropertyModelChangeProcessor = null;
        }
        mMediator.destroy();
    }
}
