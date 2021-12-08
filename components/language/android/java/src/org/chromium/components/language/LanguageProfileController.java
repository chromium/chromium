// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.language;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Controller to manage getting language preferences from device.
 */
public final class LanguageProfileController {
    private static final String TAG = "ULP";
    private static final int TIMEOUT_IN_SECONDS = 60;

    private LanguageProfileDelegate mDelegate;

    /**
     * @param delegate LanguageProfileDelegate to use.
     */
    public LanguageProfileController(LanguageProfileDelegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Get the preferred languages for user. The list is empty if an error occurs.
     * This method is blocking and must be called on a background thread.
     * @param accountName Account to get profile or null if the default profile should be returned.
     * @return A list of language tags ordered by preference for |accountName|
     */
    public List<String> getLanguagePreferences(String accountName) {
        ThreadUtils.assertOnBackgroundThread();
        if (!mDelegate.isULPSupported()) {
            // (TODO:https://crbug.com/1258261) Add initiation histogram.
            Log.d(TAG, "ULP not available");
            return new ArrayList<String>();
        }
        try {
            return mDelegate.getLanguagePreferences(accountName, TIMEOUT_IN_SECONDS);
        } catch (TimeoutException e) {
            // (TODO:https://crbug.com/1258261) Add initiation histogram.
            Log.d(TAG, "ULP getLanguagePreferences timed out");
        } catch (Exception e) {
            // (TODO:https://crbug.com/1258261) Add initiation histogram.
            Log.d(TAG, "ULP getLanguagePreferences threw exception:", e);
        }
        return new ArrayList<String>();
    }
}
