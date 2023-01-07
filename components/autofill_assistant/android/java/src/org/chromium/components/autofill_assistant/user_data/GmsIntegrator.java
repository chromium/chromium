// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.user_data;

import android.app.Activity;

import org.chromium.base.Callback;
import org.chromium.ui.base.WindowAndroid;

/**
 * Represents the interface to integrate with GMS's systems.
 *
 * <p>Warning: do not rename this class or change the signature of the non-private methods
 * (including constructor) without adapting the associated upstream code.
 */
public class GmsIntegrator {
    public GmsIntegrator(String email, Activity activity) {
        // Do nothing.
    }

    public void launchAccountIntent(
            int screenId, WindowAndroid windowAndroid, Callback<Boolean> callback) {
        callback.onResult(false);
    }

    public void launchAddInstrumentIntent(
            byte[] actionToken, WindowAndroid windowAndroid, Callback<Boolean> callback) {
        callback.onResult(false);
    }

    public void launchUpdateInstrumentIntent(
            byte[] actionToken, WindowAndroid windowAndroid, Callback<Boolean> callback) {
        callback.onResult(false);
    }

    public void launchAddressCollectionIntent(
            byte[] params, WindowAndroid windowAndroid, Callback<Boolean> callback) {
        callback.onResult(false);
    }

    public void getClientToken(Callback<byte[]> callback) {
        callback.onResult(null);
    }
}
