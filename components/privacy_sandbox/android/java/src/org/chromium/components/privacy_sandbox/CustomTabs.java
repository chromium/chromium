// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;

/* CCT-related helpers and interfaces. */
public class CustomTabs {
    /**
     * Functional interface to start a Chrome Custom Tab for the given intent, e.g. by using {@link
     * org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent}.
     */
    public interface CustomTabIntentHelper {
        /**
         * @see org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent
         */
        Intent createCustomTabActivityIntent(Context context, Intent intent);
    }

    /**
     * Opens a given Url in a CCT.
     *
     * @param customTabHelper An implementation of the {@link CustomTabIntentHelper} interface.
     * @param context A {@link Context} object.
     * @param url Url to open in the CCT.
     */
    public static void openUrlInCct(
            CustomTabIntentHelper customTabHelper, Context context, String url) {
        assert (customTabHelper != null)
                : "CCT helper must be passed to PrivacySandboxUtil before opening a link";
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();
        customTabIntent.intent.setData(Uri.parse(url));
        Intent intent =
                customTabHelper.createCustomTabActivityIntent(context, customTabIntent.intent);
        intent.setPackage(context.getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(context, intent);
    }
}
