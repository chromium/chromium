// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.language;

import java.util.ArrayList;
import java.util.List;

/** Default implementation of language profile delegate. */
public class LanguageProfileDelegateImpl implements LanguageProfileDelegate {
    /** @return True if ULP is currently supported. */
    @Override
    public boolean isULPSupported() {
        // ULP is not supported in the default implementation.
        return false;
    }

    /**
     * @param accountName Account to get profile or null if the default profile should be returned.
     * @param timeoutInSeconds Seconds to wait before timing out on call to device.
     * @return A list of language tags ordered by preference for |accountName|
     */
    @Override
    public List<String> getLanguagePreferences(String accountName, int timeoutInSeconds) {
        // The default implementation always returns an empty list.
        return new ArrayList<String>();
    }
}
