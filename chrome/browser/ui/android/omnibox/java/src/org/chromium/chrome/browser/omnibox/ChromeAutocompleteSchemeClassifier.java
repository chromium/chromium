// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.LifetimeAssert;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;

/**
 * Creates the c++ class that provides scheme classification logic for Chrome. Must call destroy()
 * after using this object to delete the native object.
 */
public class ChromeAutocompleteSchemeClassifier extends AutocompleteSchemeClassifier {
    private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    public ChromeAutocompleteSchemeClassifier(Profile profile) {
        super(ChromeAutocompleteSchemeClassifierJni.get().createAutocompleteClassifier(profile));
    }

    @Override
    public void destroy() {
        ChromeAutocompleteSchemeClassifierJni.get()
                .deleteAutocompleteClassifier(super.getNativePtr());

        // If mLifetimeAssert is GC'ed before this is called, it will throw an exception
        // with a stack trace showing the stack during LifetimeAssert.create().
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    @NativeMethods
    public interface Natives {
        long createAutocompleteClassifier(@JniType("Profile*") Profile profile);

        void deleteAutocompleteClassifier(long chromeAutocompleteSchemeClassifier);
    }
}
