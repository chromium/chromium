// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.language;

import java.util.ArrayList;
import java.util.List;

/**
 * Interface to get ULP data from GSM Core.
 */
public class LanguageProfileDelegateImpl implements LanguageProfileDelegate {
    /**
     * @return True if ULP is currently available.
     */
    @Override
    public boolean isULPAvailable() {
        // ULP is not available in the default implementation.
        return false;
    }

    /**
     * The default implementation always returns an empty list.
     * @param accountName Account to get profile or null if the default profile should be returned.
     * @return A list of language preferences for |accountName|
     */
    @Override
    public List<LanguageProfileDelegate.LanguagePreference> getLanguagePreferences(
            String accountName) {
        return new ArrayList<LanguageProfileDelegate.LanguagePreference>();
    }
}
