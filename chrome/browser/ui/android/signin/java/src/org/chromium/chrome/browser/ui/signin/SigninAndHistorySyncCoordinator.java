// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;

/**
 * Interface for coordinators responsible of showing the correct sub-component of the sign-in and
 * history opt-in flow.
 */
public interface SigninAndHistorySyncCoordinator {

    /** Cleans up the coordinator after it is finished being used. */
    void destroy();

    /**
     * Called when an Google Play Services "add account" flow started at the activity level has
     * finished without being completed.
     */
    void onAddAccountCanceled();

    /**
     * Called when an account is added via Google Play Services "add account" flow started at the
     * activity level.
     */
    void onAccountAdded(@NonNull String accountEmail);

    /** Provides the root view of the sign-in and history opt-in flow. */
    @NonNull
    View getView();

    /** Called when the configuration of the embedder activity changes. */
    void onConfigurationChange();

    /** Called when a backpress occurs in the embedder activity. */
    @BackPressResult
    int handleBackPress();
}
