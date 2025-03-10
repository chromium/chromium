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
    private final boolean mReturnShippingAddress;
    private final boolean mReturnContactInfo;

    /**
     * Constructs a mock app launcher.
     *
     * @param returnShippingAddress Whether the app should be providing user's shipping address.
     * @param returnContactInfo Whether the app should be providing user's contact information.
     */
    public MockAndroidIntentLauncher(boolean returnShippingAddress, boolean returnContactInfo) {
        mReturnShippingAddress = returnShippingAddress;
        mReturnContactInfo = returnContactInfo;
    }

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

        if (mReturnShippingAddress) {
            Bundle address = new Bundle();
            address.putString("countryCode", "CA");
            address.putStringArray("addressLines", new String[] {"111 Richmond Street West"});
            address.putString("region", "Ontario");
            address.putString("city", "Toronto");
            address.putString("postalCode", "M5H2G4");
            address.putString("recipient", "John Smith");
            address.putString("phone", "+15555555555");
            extras.putBundle("shippingAddress", address);
            extras.putString("shippingOptionId", "expressShipping");
        }

        if (mReturnContactInfo) {
            extras.putString("payerName", "John Smith");
            extras.putString("payerEmail", "John,Smith@gmail.com");
            extras.putString("payerPhone", "+15555555555");
        }

        response.putExtras(extras);
        intentCallback.onIntentCompleted(Activity.RESULT_OK, response);
    }
}
