// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Java interface that a SafeBrowsingApiHandler must implement when used with
 * {@code SafeBrowsingApiBridge}.
 */
public interface SafeBrowsingApiHandler {
    /** Observer to be notified when the SafeBrowsingApiHandler determines the verdict for a url. */
    interface Observer {
        /**
         * Called when the SafeBrowsingApiHandler gets a response from the SafeBrowsing API.
         * @param callbackId The same ID provided when {@link #startUriLookup(long, String, int[],
         *         int)} is called.
         * @param lookupResult The result of the API call. Self-defined.
         * @param threatType The threatType that is returned from the API.
         * @param threatAttributes The threatAttributes that is returned from the API.
         * @param responseStatus The responseStatus that is returned from the API.
         * @param checkDeltaMs The time the remote call took in microseconds.
         */
        void onUrlCheckDone(
                long callbackId,
                @LookupResult int lookupResult,
                int threatType,
                int[] threatAttributes,
                int responseStatus,
                long checkDeltaMs);
    }

    // Possible values for lookupResult. Native side has the same definitions. See the native side
    // definition for detailed descriptions.
    @IntDef({
        LookupResult.SUCCESS,
        LookupResult.FAILURE,
        LookupResult.FAILURE_API_CALL_TIMEOUT,
        LookupResult.FAILURE_API_UNSUPPORTED,
        LookupResult.FAILURE_API_NOT_AVAILABLE,
        LookupResult.FAILURE_HANDLER_NULL
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface LookupResult {
        int SUCCESS = 0;
        int FAILURE = 1;
        int FAILURE_API_CALL_TIMEOUT = 2;
        int FAILURE_API_UNSUPPORTED = 3;
        int FAILURE_API_NOT_AVAILABLE = 4;
        int FAILURE_HANDLER_NULL = 5;
    }

    /**
     * Start a URI-lookup to determine if the URI matches one of the threat types.
     * @param callbackId The identifier used to map the callback on the native side when the verdict
     *         is returned.
     * @param uri The URL being checked. It can be a top-level URL or a subresource URL.
     * @param threatTypes The type of threats that are checked.
     * @param protocol The protocol used to perform the check.
     */
    void startUriLookup(long callbackId, String uri, int[] threatTypes, int protocol);

    /**
     * Set the observer used to return the verdict. Must be called before {@link
     * #startUriLookup(long, String, int[], int)} is called.
     * @param observer The object on which to call the callback functions when the URI lookup is
     *         complete.
     */
    void setObserver(Observer observer);
}
