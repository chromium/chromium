// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.reload_button;

import android.widget.ImageButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.Toast;

/**
 * Root component for the reload button. Exposes public API to change button's state and allows
 * consumers to react to button state changes.
 */
@NullMarked
public class ReloadButtonCoordinator {
    /** An interface that allows parent components to control tab reload logic. */
    public interface Delegate {
        /**
         * Controls how tab is going to be reloaded.
         *
         * @param ignoreCache controls whether should force reload or not
         */
        void stopOrReloadCurrentTab(boolean ignoreCache);
    }

    private final ReloadButtonMediator mMediator;

    /**
     * Creates an instance of {@link ReloadButtonCoordinator}
     *
     * @param view reload button android view.
     * @param delegate that contains reload logic for reload button.
     */
    public ReloadButtonCoordinator(ImageButton view, ReloadButtonCoordinator.Delegate delegate) {
        final var model = new PropertyModel.Builder(ReloadButtonProperties.ALL_KEYS).build();
        mMediator =
                new ReloadButtonMediator(
                        model,
                        delegate,
                        (text) -> Toast.showAnchoredToast(view.getContext(), view, text),
                        view.getResources());
        PropertyModelChangeProcessor.create(model, view, ReloadButtonViewBinder::bind);
    }

    /**
     * Changes button reloading state.
     *
     * @param isReloading indicated whether current web page is reloading.
     */
    public void setReloading(boolean isReloading) {
        mMediator.setReloading(isReloading);
    }

    /**
     * Changes reload button enabled state.
     *
     * @param isEnabled indicates whether the button should be enabled or disabled.
     */
    public void setEnabled(boolean isEnabled) {
        mMediator.setEnabled(isEnabled);
    }

    /** Destroys current object instance. It can't be used after this call. */
    public void destroy() {
        mMediator.destroy();
    }
}
