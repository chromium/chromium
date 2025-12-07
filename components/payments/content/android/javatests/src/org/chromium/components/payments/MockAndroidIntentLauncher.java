// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.Callback;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashMap;
import java.util.Map;

/**
 * An app launcher that does not fire off any Android intents, but instead immediately returns a
 * successful intent result. Whether the resulting intent contains the shipping address or contact
 * information depends on the properties of the individual payment app.
 *
 * <p>Sample usage:
 *
 * <pre>
 *   AndroidPaymentAppFinder.setAndroidIntentLauncherForTest(
 *           new MockAndroidIntentLauncher().handleLaunchingApp(appOne).handleLaunchingApp(appTwo));
 * </pre>
 */
public class MockAndroidIntentLauncher implements AndroidIntentLauncher {
    // A mapping of payment method name (e.g., "https://payments.test/web-pay") to the corresponding
    // mock payment app.
    Map<String, MockPaymentApp> mApps = new HashMap<>();

    /**
     * Instructs the launcher to respond with {@link Activity.RESULT_OK} when launching the given
     * mock payment app. If this method is not invoked, then {@link Activity.RESULT_CANCELED} will
     * be returned instead.
     *
     * @param app The mock payment app that should be launchable. Each mock app must have a
     *     different payment method name.
     * @return A reference to this {@link MockAndroidIntentLauncher} instance.
     */
    public MockAndroidIntentLauncher handleLaunchingApp(MockPaymentApp app) {
        assert !mApps.containsKey(app.getMethod())
                : "Each mock payment app must have a different payment method name.";
        mApps.put(app.getMethod(), app);
        return this;
    }

    // AndroidIntentLauncher:
    @Override
    public void launchPaymentApp(
            Intent intent,
            Callback<String> errorCallback,
            WindowAndroid.IntentCallback intentCallback) {
        Bundle launchParameters = intent.getExtras();
        String paymentMethodName = launchParameters.getStringArrayList("methodNames").get(0);

        MockPaymentApp app = mApps.get(paymentMethodName);
        if (app == null) {
            intentCallback.onIntentCompleted(Activity.RESULT_CANCELED, null);
            return;
        }

        Bundle extras = new Bundle();
        extras.putString("methodName", paymentMethodName);
        extras.putString("details", "{\"key\": \"value\"}");

        if (app.getHandlesShippingAddress()) {
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

        if (app.getHandlesContactInformation()) {
            extras.putString("payerName", "John Smith");
            extras.putString("payerEmail", "John.Smith@gmail.com");
            extras.putString("payerPhone", "+15555555555");
        }

        Intent response = new Intent();
        response.putExtras(extras);
        intentCallback.onIntentCompleted(Activity.RESULT_OK, response);
    }
}
