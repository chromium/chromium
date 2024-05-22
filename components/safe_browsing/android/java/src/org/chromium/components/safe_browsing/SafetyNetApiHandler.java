// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Java interface that a SafetyNetApiHandler must implement when used with
 * {@code SafeBrowsingApiBridge}.
 */
public interface SafetyNetApiHandler {
    /** Observer to be notified when the SafetyNetApiHandler determines the verdict for a url. */
    interface Observer {
        // Note: |checkDelta| is the time the remote call took in microseconds.
        void onUrlCheckDone(
                long callbackId,
                @SafeBrowsingResult int resultStatus,
                String metadata,
                long checkDelta);

        void onVerifyAppsEnabledDone(long callbackId, @VerifyAppsResult int result);
    }

    // Possible values for resultStatus. Native side has the same definitions.
    @IntDef({
        SafeBrowsingResult.INTERNAL_ERROR,
        SafeBrowsingResult.SUCCESS,
        SafeBrowsingResult.TIMEOUT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface SafeBrowsingResult {
        int INTERNAL_ERROR = -1;
        int SUCCESS = 0;
        int TIMEOUT = 1;
    }

    // Values for verifyAppsResult. Native side has the same definitions.
    @IntDef({
        VerifyAppsResult.SUCCESS_ENABLED,
        VerifyAppsResult.SUCCESS_NOT_ENABLED,
        VerifyAppsResult.TIMEOUT,
        VerifyAppsResult.FAILED
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface VerifyAppsResult {
        int SUCCESS_ENABLED = 0;
        int SUCCESS_NOT_ENABLED = 1;
        int TIMEOUT = 2;
        int FAILED = 3;
    }

    /**
     * Verifies that SafetyNetApiHandler can operate and initializes if feasible. Should be called
     * on the same sequence as |startUriLookup|.
     *
     * @param observer The object on which to call the callback functions when URL checking is
     *     complete.
     * @return whether Safe Browsing is supported for this installation.
     */
    boolean init(Observer observer);

    /**
     * Start a URI-lookup to determine if it matches one of the specified threats.
     * This is called on every URL resource Chrome loads, on the same sequence as |init|.
     */
    void startUriLookup(long callbackId, String uri, int[] threatsOfInterest);

    /**
     * Start a check to determine if a uri is in an allowlist. If true, password protection service
     * will consider the uri to be safe.
     *
     * @param uri The uri from a password protection event(user focuses on password form * or user
     *     reuses their password)
     * @param threatType determines the type of the allowlist that the uri will be matched to.
     * @return true if the uri is found in the corresponding allowlist. Otherwise, false.
     */
    boolean startAllowlistLookup(String uri, int threatType);

    /**
     * Start a check to see if the user has app verification enabled. The response will be provided
     * to the observer with the onVerifyAppsEnabledDone method.
     *
     * @param callbackId The id of the callback which should be returned * with the result.
     */
    // TODO(crbug.com/341790041): Remove the default implementations on
    // real ones have landed. These ones are not suitable for production
    // use.
    default void isVerifyAppsEnabled(long callbackId) {}

    /**
     * Prompt the user to enable app verification. The response will be provided to the observer
     * with the onVerifyAppsEnabledDone method.
     *
     * @param callbackId The id of the callback which should be returned * with the result.
     */
    // TODO(crbug.com/341790041): Remove the default implementations on
    // real ones have landed. These ones are not suitable for production
    // use.
    default void enableVerifyApps(long callbackId) {}
}
