// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import androidx.browser.customtabs.CustomTabsService;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifierFactory;
import org.chromium.components.embedder_support.util.Origin;

import java.util.List;

/**
 * A utility class to verify Digital Asset Links (DAL) between an Android package and a web Origin.
 */
@NullMarked
public class DigitalAssetLinksVerifier {
    /**
     * Checks a list of packages and returns the index of the first one that is verified to have a
     * "use_as_origin" digital asset link with the given origin. If none match, returns null.
     */
    public static void checkPackages(
            List<String> packages, Origin origin, Callback<Integer> callback) {
        checkPackages(packages, origin, 0, callback);
    }

    private static void checkPackages(
            List<String> packages, Origin origin, int index, Callback<Integer> callback) {
        if (packages == null || index >= packages.size()) {
            callback.onResult(-1);
            return;
        }

        verifyAsOrigin(
                packages.get(index),
                origin,
                verified -> {
                    if (verified) {
                        callback.onResult(index);
                    } else {
                        checkPackages(packages, origin, index + 1, callback);
                    }
                });
    }

    /**
     * Verifies if the given package has a "use_as_origin" digital asset link with the given origin.
     */
    public static void verifyAsOrigin(
            String packageName, Origin origin, Callback<Boolean> callback) {
        ChromeOriginVerifier verifier =
                ChromeOriginVerifierFactory.create(
                        packageName, CustomTabsService.RELATION_USE_AS_ORIGIN, null);

        verifier.start(
                (pkgName, verifiedOrigin, verified, online) -> {
                    callback.onResult(verified);
                },
                origin);
    }
}
