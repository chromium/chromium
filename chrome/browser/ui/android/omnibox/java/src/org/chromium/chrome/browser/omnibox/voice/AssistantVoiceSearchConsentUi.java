// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

/**
 * AssistantVoiceSearchConsentUi provides a common interface for interacting with consent UIs.
 */
public interface AssistantVoiceSearchConsentUi {
    /**
     * An observer used to notify the UI controller that a consent outcome has been reached.
     *
     * Exactly one of the terminating methods (onConsentAccepted, onConsentRejected,
     * onConsentCanceled, onNonUserCancel) should be called on the observer when the UI is acted
     * upon.
     */
    public interface Observer {
        // The consent was accepted.
        void onConsentAccepted();
        // The consent was rejected.
        void onConsentRejected();
        // The consent was canceled (backed out of, swiped away, etc).
        void onConsentCanceled();
        // The consent UI was closed for a non-user-initiated reason.
        void onNonUserCancel();

        // onLearnMoreClicked is non-terminating. The observer should still expect an eventual call
        // to one of the above result methods.
        void onLearnMoreClicked();
    }

    /**
     * Show the consent UI. The given observer will be used to report consent outcomes and will be
     * released after either an outcome is reported or dismiss() is called.
     */
    void show(Observer observer);

    /**
     * Hide the consent UI. No Observer methods should be called in this case, and the observer will
     * be released. This can be called in the case where a consent outcome was reached outside of
     * the UI, e.g. via settings.
     */
    void dismiss();
}
