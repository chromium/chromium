// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Interface representing the host (display channel) for the enterprise signals disclaimer. It
 * abstracts the display logic which could be a Bottom Sheet or a Modal Dialog.
 */
@NullMarked
interface EnterpriseSignalsDisclaimerHost {

    @IntDef({
        DismissalCause.TAPPED_ACCEPT,
        DismissalCause.TAPPED_SIGN_OUT,
        DismissalCause.DISMISSED_BY_BACK_PRESS,
        DismissalCause.DISMISSED_BY_SWIPE_DOWN,
        DismissalCause.DISMISSED_BY_TAP_OUTSIDE,
        DismissalCause.DISMISSED_WITHOUT_EXPLICIT_USER_ACTION,
        DismissalCause.DISMISSED_BY_CLOSE_BUTTON,
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    public @interface DismissalCause {
        /** The user explicitly accepted the disclaimer. */
        int TAPPED_ACCEPT = 0;

        /** The user explicitly clicked the sign out button. */
        int TAPPED_SIGN_OUT = 1;

        /** The user dismissed the disclaimer by pressing the back button. */
        int DISMISSED_BY_BACK_PRESS = 2;

        /** The user dismissed the disclaimer by swiping it down - Bottom Sheet only. */
        int DISMISSED_BY_SWIPE_DOWN = 3;

        /** The user dismissed the disclaimer by tapping outside of the dialog. */
        int DISMISSED_BY_TAP_OUTSIDE = 4;

        /** The user dismissed the disclaimer by clicking the close button - Bottom Sheet only. */
        int DISMISSED_BY_CLOSE_BUTTON = 5;

        /** The dismissal was not directly caused by a user action. */
        int DISMISSED_WITHOUT_EXPLICIT_USER_ACTION = 6;
    }

    /**
     * Attempts to show the enterprise signals disclaimer. If the dialog cannot be shown it will be
     * put in a queue and shown whenever possible.
     */
    void show();

    /**
     * @return true if dialog is being shown or is in queue, false otherwise.
     */
    boolean isActive();

    /**
     * Dismisses the disclaimer UI.
     *
     * @param dismissalCause The cause of the dismissal.
     */
    void dismiss(@DismissalCause int dismissalCause);

    /** Stops observation and dismisses the dialog. */
    void destroy();
}
