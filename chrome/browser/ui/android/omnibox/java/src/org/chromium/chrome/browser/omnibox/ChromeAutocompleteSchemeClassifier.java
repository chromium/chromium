// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.base.LifetimeAssert;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;

/**
 * Creates the c++ class that provides scheme classification logic for Chrome.
 * Must call destroy() after using this object to delete the native object.
 */
public class ChromeAutocompleteSchemeClassifier extends AutocompleteSchemeClassifier {
    private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    public ChromeAutocompleteSchemeClassifier(Profile profile) {
        super(ChromeAutocompleteSchemeClassifierJni.get().createAutocompleteClassifier(profile));
    }

    @Override
    public void destroy() {
        ChromeAutocompleteSchemeClassifierJni.get().deleteAutocompleteClassifier(
                super.getNativePtr());

        // If mLifetimeAssert is GC'ed before this is called, it will throw an exception
        // with a stack trace showing the stack during LifetimeAssert.create().
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    @NativeMethods
    interface Natives {
        long createAutocompleteClassifier(Profile profile);
        void deleteAutocompleteClassifier(long chromeAutocompleteSchemeClassifier);
    }
}
