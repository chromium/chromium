// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.onboarding;

import org.chromium.base.Callback;

/**
 * Interface for a Java-side autofill assistant onboarding coordinator.
 */
public interface OnboardingView {
    /**
     * Shows onboarding and provides the result to the given callback.
     *
     * <p>The {@code callback} will be called when the user accepts, cancels or dismisses the
     * onboarding.
     *
     * @param callback Callback to report when user accepts or cancels the onboarding.
     */
    void show(Callback<Integer> callback);

    /** Hides the onboarding UI, if one is shown. */
    void hide();

    /** Updates the contents of views based on the presence of parameters and experiments. */
    void updateViews();

    /**
     * Returns {@code true} if the onboarding has been shown at the beginning when this
     * autofill assistant flow got triggered.
     *
     * @return Whether the onboarding screen has been shown to the user.
     */
    boolean getOnboardingShown();
}