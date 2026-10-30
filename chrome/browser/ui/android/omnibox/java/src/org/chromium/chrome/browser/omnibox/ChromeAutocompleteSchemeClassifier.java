// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.jni_zero.JniType;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;

/**
 * Creates the c++ class that provides scheme classification logic for Chrome. Must call destroy()
 * after using this object to delete the native object.
 */
@NullMarked
public class ChromeAutocompleteSchemeClassifier extends AutocompleteSchemeClassifier {
    public ChromeAutocompleteSchemeClassifier(Profile profile) {
        super(ChromeAutocompleteSchemeClassifierJni.get().createAutocompleteClassifier(profile));
    }

    @NativeMethods
    public interface Natives {
        JniUniquePtr<AutocompleteSchemeClassifier.NativeAutocompleteSchemeClassifier>
                createAutocompleteClassifier(@JniType("Profile*") Profile profile);
    }
}
