// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

import java.util.List;

/** A class used to record journey metrics for the Payment Request feature. */
@JNINamespace("payments")
public class JourneyLogger {
    /** Pointer to the native implementation. */
    private long mJourneyLoggerAndroid;

    private boolean mHasRecorded;

    /**
     * Creates the journey logger.
     *
     * @param webContents The web contents where PaymentRequest API is invoked. Should not be null.
     */
    public JourneyLogger(WebContents webContents) {
        assert webContents != null;
        assert !webContents.isDestroyed();
        // Note that this pointer could leak the native object. The called must call destroy() to
        // ensure that the native object is destroyed.
        mJourneyLoggerAndroid =
                JourneyLoggerJni.get().initJourneyLoggerAndroid(JourneyLogger.this, webContents);
    }

    /** Will destroy the native object. This class shouldn't be used afterwards. */
    public void destroy() {
        if (mJourneyLoggerAndroid != 0) {
            JourneyLoggerJni.get().destroy(mJourneyLoggerAndroid, JourneyLogger.this);
            mJourneyLoggerAndroid = 0;
        }
    }

    /**
     * Sets the number of suggestions shown for the specified section.
     *
     * @param section               The section for which to log.
     * @param number                The number of suggestions.
     * @param hasCompleteSuggestion Whether the section has at least one
     *                              complete suggestion.
     */
    public void setNumberOfSuggestionsShown(
            int section, int number, boolean hasCompleteSuggestion) {
        assert section < Section.MAX;
        JourneyLoggerJni.get()
                .setNumberOfSuggestionsShown(
                        mJourneyLoggerAndroid,
                        JourneyLogger.this,
                        section,
                        number,
                        hasCompleteSuggestion);
    }

    /** Records that an Opt Out experience is being offered to the user in the current UI flow. */
    public void setOptOutOffered() {
        JourneyLoggerJni.get().setOptOutOffered(mJourneyLoggerAndroid, JourneyLogger.this);
    }

    /** Records that a PaymentRequest.show() was allowed without a user activation. */
    public void setActivationlessShow() {
        JourneyLoggerJni.get().setActivationlessShow(mJourneyLoggerAndroid, JourneyLogger.this);
    }

    /** Records that a payment app has been invoked without any payment UI being shown before that. */
    public void setSkippedShow() {
        JourneyLoggerJni.get().setSkippedShow(mJourneyLoggerAndroid, JourneyLogger.this);
    }

    /** Records that a payment UI has been shown. */
    public void setShown() {
        JourneyLoggerJni.get().setShown(mJourneyLoggerAndroid, JourneyLogger.this);
    }

    /** Records that a payment app was invoked. */
    public void setPayClicked() {
        JourneyLoggerJni.get().setPayClicked(mJourneyLoggerAndroid, JourneyLogger.this);
    }

    /**
     * Records the method that has been selected and invoked.
     *
     * @param category The category of the method.
     */
    public void setSelectedMethod(@PaymentMethodCategory int category) {
        JourneyLoggerJni.get()
                .setSelectedMethod(mJourneyLoggerAndroid, JourneyLogger.this, category);
    }

    /*
     * Records what user information were requested by the merchant to complete the Payment Request.
     *
     * @param requestShipping Whether the merchant requested a shipping address.
     * @param requestEmail    Whether the merchant requested an email address.
     * @param requestPhone    Whether the merchant requested a phone number.
     * @param requestName     Whether the merchant requestes a name.
     */
    public void setRequestedInformation(
            boolean requestShipping,
            boolean requestEmail,
            boolean requestPhone,
            boolean requestName) {
        JourneyLoggerJni.get()
                .setRequestedInformation(
                        mJourneyLoggerAndroid,
                        JourneyLogger.this,
                        requestShipping,
                        requestEmail,
                        requestPhone,
                        requestName);
    }

    /*
     * Records what types of payment methods were requested by the merchant in the Payment Request.
     *
     * @param methodTypes The list of types of the payment methods, defined in
     *        {@link PaymentMethodCategories}.
     */
    public void setRequestedPaymentMethods(List<Integer> methodTypes) {
        int[] methods = new int[methodTypes.size()];
        for (int i = 0; i < methodTypes.size(); i++) {
            methods[i] = methodTypes.get(i);
        }
        JourneyLoggerJni.get()
                .setRequestedPaymentMethods(mJourneyLoggerAndroid, JourneyLogger.this, methods);
    }

    /**
     * Records that the Payment Request was completed successfully. Also starts the logging of
     * all the journey logger metrics.
     */
    public void setCompleted() {
        assert !mHasRecorded;

        if (!mHasRecorded) {
            mHasRecorded = true;
            JourneyLoggerJni.get().setCompleted(mJourneyLoggerAndroid, JourneyLogger.this);
        }
    }

    /**
     * Records that the Payment Request was aborted. This counts as a completion, starting the
     * logging of all the journey metrics.
     *
     * @param reason An int indicating why the payment request was aborted.
     */
    public void setAborted(int reason) {
        assert reason < AbortReason.MAX;

        // The abort reasons on Android cascade into each other, so only the first one should be
        // recorded.
        if (!mHasRecorded) {
            mHasRecorded = true;
            JourneyLoggerJni.get().setAborted(mJourneyLoggerAndroid, JourneyLogger.this, reason);
        }
    }

    /** Records that the Payment Request was not shown to the user. */
    public void setNotShown() {
        assert !mHasRecorded;

        if (!mHasRecorded) {
            mHasRecorded = true;
            JourneyLoggerJni.get().setNotShown(mJourneyLoggerAndroid, JourneyLogger.this);
        }
    }

    /** Records that the No Matching Credentials UX was shown to the user. */
    public void setNoMatchingCredentialsShown() {
        JourneyLoggerJni.get()
                .setNoMatchingCredentialsShown(mJourneyLoggerAndroid, JourneyLogger.this);
    }

    /**
     * Records that the payment request has entered the given checkout step.
     * @param step An int indicating the step to be recorded.
     */
    public void recordCheckoutStep(int step) {
        JourneyLoggerJni.get().recordCheckoutStep(mJourneyLoggerAndroid, JourneyLogger.this, step);
    }

    /**
     * Sets the ukm source id of payment app.
     * @param sourceId A long indicating the ukm source id of the invoked payment app.
     */
    public void setPaymentAppUkmSourceId(long sourceId) {
        JourneyLoggerJni.get()
                .setPaymentAppUkmSourceId(mJourneyLoggerAndroid, JourneyLogger.this, sourceId);
    }

    @NativeMethods
    interface Natives {
        long initJourneyLoggerAndroid(JourneyLogger caller, WebContents webContents);

        void destroy(long nativeJourneyLoggerAndroid, JourneyLogger caller);

        void setNumberOfSuggestionsShown(
                long nativeJourneyLoggerAndroid,
                JourneyLogger caller,
                int section,
                int number,
                boolean hasCompleteSuggestion);

        void setOptOutOffered(long nativeJourneyLoggerAndroid, JourneyLogger caller);

        void setActivationlessShow(long nativeJourneyLoggerAndroid, JourneyLogger caller);

        void setSkippedShow(long nativeJourneyLoggerAndroid, JourneyLogger caller);

        void setShown(long nativeJourneyLoggerAndroid, JourneyLogger caller);

        void setPayClicked(long nativeJourneyLoggerAndroid, JourneyLogger caller);

        void setSelectedMethod(
                long nativeJourneyLoggerAndroid, JourneyLogger caller, int paymentMethodCategory);

        void setRequestedInformation(
                long nativeJourneyLoggerAndroid,
                JourneyLogger caller,
                boolean requestShipping,
                boolean requestEmail,
                boolean requestPhone,
                boolean requestName);

        void setRequestedPaymentMethods(
                long nativeJourneyLoggerAndroid, JourneyLogger caller, int[] methodTypes);

        void setCompleted(long nativeJourneyLoggerAndroid, JourneyLogger caller);

        void setAborted(long nativeJourneyLoggerAndroid, JourneyLogger caller, int reason);

        void setNotShown(long nativeJourneyLoggerAndroid, JourneyLogger caller);

        void setNoMatchingCredentialsShown(long nativeJourneyLoggerAndroid, JourneyLogger caller);

        void recordCheckoutStep(long nativeJourneyLoggerAndroid, JourneyLogger caller, int step);

        void setPaymentAppUkmSourceId(
                long nativeJourneyLoggerAndroid, JourneyLogger caller, long sourceId);
    }
}
