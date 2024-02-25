// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import android.app.PendingIntent;
import android.content.Intent;

/** Stores information about a particular app. */
public class AppData {
    // Immutable data about this app.
    private final String mSiteUrl;
    private final String mPackageName;

    // Data returned by the system when queried about the app.
    private String mTitle;
    private String mImageUrl;
    private float mRating;
    private String mInstallButtonText;
    private PendingIntent mDetailsIntent;
    private Intent mInstallIntent;

    /**
     * Creates a new AppData for the given page and package.
     *
     * @param siteUrl URL for the site requesting the banner.
     * @param packageName Name of the package associated with the app.
     */
    public AppData(String siteUrl, String packageName) {
        mSiteUrl = siteUrl;
        mPackageName = packageName;
    }

    /**
     * Returns the URL of the website requesting the banner.
     *
     * @return The URL of the website.
     */
    public String siteUrl() {
        return mSiteUrl;
    }

    /**
     * Returns the package name of the app.
     *
     * @return The String containing the package name.
     */
    public String packageName() {
        return mPackageName;
    }

    /**
     * Returns the title to display for the app in the banner.
     *
     * @return The String to display.
     */
    public String title() {
        return mTitle;
    }

    /**
     * Returns the URL where the app icon can be retrieved from.
     *
     * @return The URL to grab the icon from.
     */
    public String imageUrl() {
        return mImageUrl;
    }

    /**
     * Returns how well the app was rated, on a scale from 0 to 5.
     *
     * @return The rating of the app.
     */
    public float rating() {
        return mRating;
    }

    /**
     * Returns text to display on the install button when the app is not installed on the system.
     *
     * @return The String to display.
     */
    public String installButtonText() {
        return mInstallButtonText;
    }

    /**
     * Returns the Intent used to send a user to a details page about the app. The IntentSender
     * stored inside dictates what package needs to be launched.
     *
     * @return Intent that triggers the details page.
     */
    public PendingIntent detailsIntent() {
        return mDetailsIntent;
    }

    /**
     * Returns the Intent that triggers the install.
     *
     * @return Intent used to trigger the install.
     */
    public Intent installIntent() {
        return mInstallIntent;
    }

    /**
     * Stores all of the data about the given app after it's been retrieved.
     *
     * @param title App title.
     * @param imageUrl URL where the icon is located.
     * @param rating Rating of the app.
     * @param installButtonText Text to display on the install button if it's not installed yet.
     * @param detailsIntent Intent to fire to launch the details page for the app
     * @param installIntent Intent to fire to trigger the purchase/install process.
     */
    public void setPackageInfo(
            String title,
            String imageUrl,
            float rating,
            String installButtonText,
            PendingIntent detailsIntent,
            Intent installIntent) {
        mTitle = title;
        mImageUrl = imageUrl;
        mRating = rating;
        mInstallButtonText = installButtonText;
        mDetailsIntent = detailsIntent;
        mInstallIntent = installIntent;
    }
}
