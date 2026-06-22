// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;

/** Interface representing the bottom bar. */
@NullMarked
public interface BottomBar {
    /** Returns the view representing the bottom bar. */
    View getView();

    /** Returns the background color of the bottom bar. */
    @ColorInt
    int getBackgroundColor();

    /**
     * Informs the bottom bar that its parent has changed.
     *
     * @param host The new host of the bottom bar.
     */
    void setParent(@Host int host);

    /** Evaluates whether to show the introductory promo dialog and shows it if eligible. */
    boolean maybeShowPromoDialog(Profile profile);

    /**
     * Notifies the bottom bar that the startup promo flow has finished.
     *
     * @param promoShown True if any startup promo (or required prompt) was shown to the user during
     *     the startup flow. If true, the bottom bar should suppress showing its own in-product help
     *     (IPH) immediately to avoid overwhelming the user.
     */
    void onStartupPromoFlowFinished(boolean promoShown);
}
