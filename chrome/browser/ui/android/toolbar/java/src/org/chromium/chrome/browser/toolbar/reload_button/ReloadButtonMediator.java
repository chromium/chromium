// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.reload_button;

import android.animation.ObjectAnimator;
import android.content.res.ColorStateList;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.KeyboardNavigationListener;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A Mediator for reload button. Glues reload button and external events to the model that relays
 * them back to the reload button state.
 */
@NullMarked
class ReloadButtonMediator implements ThemeColorProvider.TintObserver {

    // TODO(vkorotkevich): suppression will be removed in the follow up CLs
    @SuppressWarnings("unused")
    private final PropertyModel mModel;

    /**
     * Create an instance of {@link ReloadButtonMediator}.
     *
     * @param model a properties model that encapsulates reload button state.
     */
    ReloadButtonMediator(PropertyModel model) {
        mModel = model;
    }

    @Override
    public void onTintChanged(
            ColorStateList tint, ColorStateList activityFocusTint, int brandedColorScheme) {}

    /**
     * Creates a show/hide animator that animates view's alpha.
     *
     * @param isShowing indicated fade in or out animation type
     * @return {@link ObjectAnimator} that animates view's alpha
     */
    // TODO(vkorotkevich): Remove @Nullable
    public @Nullable ObjectAnimator getFadeAnimator(boolean isShowing) {
        return null;
    }

    /**
     * Changes reload button state to reload or stopped.
     *
     * @param isReloading indicates whether current page is reloading.
     */
    public void setReloading(boolean isReloading) {}

    /**
     * Sets reload button visibility.
     *
     * @param isVisible indicated whether view should be visible or gone.
     */
    public void setVisibility(boolean isVisible) {}

    /**
     * Sets a listeners that allows parent to intercept keyboard navigation events.
     *
     * @param listener {@link KeyboardNavigationListener}
     */
    public void setKeyboardNavigationListener(KeyboardNavigationListener listener) {}

    public void destroy() {}
}
