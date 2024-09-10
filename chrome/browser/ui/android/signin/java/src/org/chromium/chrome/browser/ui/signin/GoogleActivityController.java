// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Controls how Google uses Chrome data to personalize Search and other Google services. */
public class GoogleActivityController {

    public static GoogleActivityController create() {
        GoogleActivityController ret =
                ServiceLoaderUtil.maybeCreate(GoogleActivityController.class);
        if (ret == null) {
            ret = new GoogleActivityController();
        }
        return ret;
    }

    /**
     * Opens the "Web & App Activity" settings that allows the user to control how Google uses
     * Chrome browsing history.
     *
     * @param activity The activity to open the settings.
     * @param accountName The account for which is requested.
     */
    public void openWebAndAppActivitySettings(Activity activity, String accountName) {
        Intent intent =
                new Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse(UrlConstants.GOOGLE_ACCOUNT_ACTIVITY_CONTROLS_URL));
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, activity.getPackageName());
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        intent.setPackage(activity.getPackageName());
        activity.startActivity(intent);
    }

    /**
     * Opens the "Linked Google services" settings that allows the user to control with what other
     * Google Services Chrome can share data.
     *
     * @param activity The activity to open the settings.
     * @param accountName The account for which is requested.
     */
    public void openLinkedGoogleServicesSettings(Activity activity, String accountName) {
        Intent intent =
                new Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse(UrlConstants.GOOGLE_ACCOUNT_LINKED_SERVICES_URL));
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, activity.getPackageName());
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        intent.setPackage(activity.getPackageName());
        activity.startActivity(intent);
    }
}
