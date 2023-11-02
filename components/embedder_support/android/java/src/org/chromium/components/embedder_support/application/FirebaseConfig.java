// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.application;

import org.chromium.build.BuildConfig;

/**
 * Simple class containing build specific Firebase app IDs.
 */
public final class FirebaseConfig {
    private FirebaseConfig() {}

    /**
     * Get the Firebase app ID that should be uploaded with crashes to enable deobfuscation.
     * See http://goto.google.com/clank/engineering/sdk-build/proguard for more info.
     *
     * @return the Firebase app ID.
     */
    public static String getFirebaseAppIdForPackage(String packageName) {
        if (!BuildConfig.IS_CHROME_BRANDED || packageName == null) return "";

        switch (packageName) {
            case "com.chrome.canary":
                return "1:850546144789:android:54d7b17bce961ff1";
            case "com.chrome.dev":
                return "1:573639067789:android:51b6bb8c28a80880";
            case "com.chrome.beta":
                return "1:555018597840:android:685a0b5814643d3e";
            case "com.android.chrome":
                return "1:914760932289:android:bdb905fe8b8890ae";
            case "com.google.android.webview.canary":
                return "1:767306611607:android:313882f0a045ab2f4da2d2";
            case "com.google.android.webview":
                return "1:885372672379:android:e1ff119a3219cbe0";
            case "com.google.android.webview.beta":
                return "1:352610373116:android:4f951c858c7e5cf8e7f679";
            case "com.google.android.webview.dev":
                return "1:366353795560:android:935a11469683e358b27c5d";
            default:
                return "";
        }
    }
}