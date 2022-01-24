// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.language;

import java.util.List;

/**
 * Interface to get language profile data for device.
 */
public interface LanguageProfileDelegate {
    /**
     * Helper class to hold language preference values.
     */
    public static class LanguagePreference {
        private final String mLanguageName;
        private final float mPreference;

        public LanguagePreference(String languageName, float preference) {
            mLanguageName = languageName;
            mPreference = preference;
        }

        public String getLanguage() {
            return mLanguageName;
        }

        public float getPreference() {
            return mPreference;
        }
    }

    /**
     * @return True if ULP is currently available.
     */
    public boolean isULPAvailable();

    /**
     * @param accountName Account to get profile or null if the default profile should be returned.
     * @return A list of language preferences for |accountName|
     */
    public List<LanguagePreference> getLanguagePreferences(String accountName);
}
