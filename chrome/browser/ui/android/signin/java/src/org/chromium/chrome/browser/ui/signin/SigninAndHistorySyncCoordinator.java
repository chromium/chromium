// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Interface for coordinators responsible of showing the correct sub-component of the sign-in and
 * history opt-in flow.
 */
public interface SigninAndHistorySyncCoordinator {

    /** Indicates the sign-in flow completion status. */
    @IntDef({
        Result.COMPLETED,
        Result.INTERRUPTED,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface Result {
        /** Indicates the sign-in flow completed successfully. */
        int COMPLETED = 0;

        /**
         * Indicates the sign-in flow was not completed due to error. The conditions depend on the
         * configuration of the sign-in flow: e.g. if history opt-in is shown, declining history
         * opt-in will set the INTERRUPTED state, and same for the sign-in step.
         */
        int INTERRUPTED = 1;
    }

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
