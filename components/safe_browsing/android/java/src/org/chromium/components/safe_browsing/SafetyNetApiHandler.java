// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

/**
 * Java interface that a SafetyNetApiHandler must implement when used with {@code
 * SafeBrowsingApiBridge}.
 */
@NullMarked
public interface SafetyNetApiHandler {
    // Enumerates possible initialization states for the SafetyNetApiHandler.
    @IntDef({
        SafetyNetApiState.NOT_AVAILABLE,
        SafetyNetApiState.INITIALIZED,
        SafetyNetApiState.INITIALIZED_FIRST_PARTY,
    })
    @interface SafetyNetApiState {
        // The API handler is not initialized. Calls to methods below will not work.
        int NOT_AVAILABLE = 0;
        // The API handler is initialized for most usages, but calls to {@link getSafetyNetId} will
        // not work.
        int INITIALIZED = 1;
        // The API handler is initialized for all method calls.
        int INITIALIZED_FIRST_PARTY = 2;
    };

    /**
     * Observer to be notified when the SafetyNetApiHandler determines the result of asynchronous
     * calls.
     */
    interface Observer {
        void onVerifyAppsEnabledDone(long callbackId, @VerifyAppsResult int result);

        // (TODO:crbug.com/449183636) Remove this overload once internal implementation is changed.
        void onHasHarmfulAppsDone(
                long callbackId, @HasHarmfulAppsResultStatus int result, int numberOfApps);

        void onHasHarmfulAppsDone(
                long callbackId,
                @HasHarmfulAppsResultStatus int result,
                int numberOfApps,
                int statusCode);

        void onGetSafetyNetIdDone(String result);
    }

    /**
     * Verifies that SafetyNetApiHandler can operate and initializes if feasible. Should be called
     * on the same sequence as {@link startAllowlistLookup}, {@link isVerifyAppsEnabled}, {@link
     * hasPotentiallyHarmfulApps}, and {@link getSafetyNetId}.
     *
     * @param observer The object on which to call the callback functions when app verification
     *     checking is complete.
     * @return Enum value indicating which methods are supported for this installation.
     */
    @SafetyNetApiState
    int initialize(Observer observer);

    /**
     * Start a check to determine if a uri is in an allowlist. If true, Safe Browsing will consider
     * the uri to be safe. Requires initialized state {@code INITIALIZED} or {@code
     * INITIALIZED_FIRST_PARTY}.
     *
     * @param uri The uri from a safe browsing relevant event (for password protection: user focuses
     *     on password form or user reuses their password; for download protection: a downloaded
     *     file of appropriate filetype).
     * @param threatType determines the type of the allowlist that the uri will be matched to.
     * @return true if the uri is found in the corresponding allowlist. Otherwise, false.
     */
    boolean startAllowlistLookup(String uri, int threatType);

    /**
     * Start a check to see if the user has app verification enabled. The response will be provided
     * to the observer with the onVerifyAppsEnabledDone method. Requires initialized state {@code
     * INITIALIZED} or {@code INITIALIZED_FIRST_PARTY}.
     *
     * @param callbackId The id of the callback which should be returned with the result.
     */
    void isVerifyAppsEnabled(long callbackId);

    /**
     * Prompt the user to enable app verification. The response will be provided to the observer
     * with the onVerifyAppsEnabledDone method. Requires initialized state {@code INITIALIZED} or
     * {@code INITIALIZED_FIRST_PARTY}.
     *
     * @param callbackId The id of the callback which should be returned with the result.
     */
    void enableVerifyApps(long callbackId);

    // TODO(crbug.com/446681100): Remove the default once internal implementation is complete.
    /**
     * Start a check to see if there is any potentially harmful apps present. The response will be
     * provided to the observer with the onHasHarmfulAppsDone method. Requires initialized state
     * {@code INITIALIZED} or {@code INITIALIZED_FIRST_PARTY}.
     *
     * @param callbackId The id of the callback which should be returned with the result.
     */
    default void hasPotentiallyHarmfulApps(long callbackId) {}

    /**
     * Get the shared UUID from the SafetyNet API. A result will be returned to {@link
     * Observer#onGetSafetyNetIdDone}. Result may be empty string in case of error, in which case
     * the error is not likely recoverable during this process lifetime. May also return a non-empty
     * default value for some errors. Requires initialized state {@code INITIALIZED_FIRST_PARTY}.
     */
    void getSafetyNetId();
}
