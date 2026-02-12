// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import org.chromium.base.Promise;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

@NullMarked
public class IdentityCredentialsDelegate {
    private static final String TAG = "IdentityCredentials";

    public static class DigitalCredential {
        public String mProtocol;
        public String mData;

        public DigitalCredential(String protocol, String data) {
            this.mProtocol = protocol;
            this.mData = data;
        }
    }

    public @Nullable Promise<String> get(String origin, String request) {
        // TODO(crbug.com/40257092): implement this.
        return null;
    }

    public Promise<DigitalCredential> get(WindowAndroid window, String origin, String request) {
        DigitalCredentialsPresentationDelegate presentationDelegate =
                new DigitalCredentialsPresentationDelegate();
        return presentationDelegate.get(window, origin, request);
    }

    public Promise<DigitalCredential> create(WindowAndroid window, String origin, String request) {
        DigitalCredentialsCreationDelegate creationDelegate =
                new DigitalCredentialsCreationDelegate();
        return creationDelegate.create(window, origin, request);
    }
}
