// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.fusebox.Fusebox.PopupState;
import org.chromium.components.omnibox.OmniboxFocusReason;

/**
 * Tracks whether the active Fusebox input session is in guided tour mode.
 *
 * <p>Modeled as a two-state machine (inactive / active). While inactive, no observers are attached.
 * Starting a guided tour input transitions to active and observes the Fusebox popup; hiding the
 * popup or ending the session detaches the observer and returns to inactive.
 */
@NullMarked
class FuseboxGuidedTourController {
    private final NonNullObservableSupplier<@PopupState Integer> mPopupStateSupplier;
    private final Callback<@PopupState Integer> mPopupStateObserver = this::onPopupStateChanged;
    private boolean mIsGuidedTourModeActive;

    FuseboxGuidedTourController(NonNullObservableSupplier<@PopupState Integer> popupStateSupplier) {
        mPopupStateSupplier = popupStateSupplier;
    }

    /** Returns whether guided tour mode is active. */
    boolean isGuidedTourModeActive() {
        return mIsGuidedTourModeActive;
    }

    /** Updates guided tour mode for a newly started input session. */
    void beginInput(@OmniboxFocusReason int focusReason) {
        endTour();
        if (focusReason != OmniboxFocusReason.FUSEBOX_GUIDED_TOUR) {
            return;
        }
        mPopupStateSupplier.addSyncObserver(mPopupStateObserver);
        mIsGuidedTourModeActive = true;
    }

    /** Exits guided tour mode when the input session ends. */
    void endInput() {
        endTour();
    }

    void destroy() {
        endTour();
    }

    private void onPopupStateChanged(@PopupState int popupState) {
        if (popupState == PopupState.HIDDEN) {
            endTour();
        }
    }

    private void endTour() {
        if (!mIsGuidedTourModeActive) {
            return;
        }
        mPopupStateSupplier.removeObserver(mPopupStateObserver);
        mIsGuidedTourModeActive = false;
    }
}
