// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.Callback;
import org.chromium.ui.base.WindowAndroid;

/**
 * An app launcher that does not fire off any Android intents, but instead immediately returns a
 * successful intent result.
 */
public class MockAndroidIntentLauncher implements AndroidIntentLauncher {
    @Override
    public void launchPaymentApp(
            Intent intent,
            Callback<String> errorCallback,
            WindowAndroid.IntentCallback intentCallback) {
        Bundle launchParameters = intent.getExtras();
        String paymentMethodName = launchParameters.getStringArrayList("methodNames").get(0);

        Intent response = new Intent();
        Bundle extras = new Bundle();
        extras.putString("methodName", paymentMethodName);
        extras.putString("details", "{\"key\": \"value\"}");
        response.putExtras(extras);
        intentCallback.onIntentCompleted(Activity.RESULT_OK, response);
    }
}
