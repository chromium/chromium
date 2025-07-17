// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.facilitated_payments;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;

import org.chromium.base.PackageManagerUtils;
import org.chromium.build.annotations.NullMarked;

import java.util.List;

/** Abstraction of Android's package manager to enable testing. */
@NullMarked
public class PackageManagerDelegate {
    private final PackageManager mPackageManager;

    PackageManagerDelegate(PackageManager packageManager) {
        mPackageManager = packageManager;
    }

    /**
     * Retrieves the list of activities that can respond to the given intent.
     *
     * @param intent The intent to query.
     * @return The list of activities that can respond to the intent.
     */
    public List<ResolveInfo> getActivitiesThatCanRespondToIntent(Intent intent) {
        return PackageManagerUtils.queryIntentActivities(intent, 0);
    }

    /**
     * Retrieves the label of the app.
     *
     * @param resolveInfo The identifying information for an app.
     * @return The label for this app.
     */
    public CharSequence getAppLabel(ResolveInfo resolveInfo) {
        return resolveInfo.loadLabel(mPackageManager);
    }

    /**
     * Retrieves the icon of the app.
     *
     * @param resolveInfo The identifying information for an app.
     * @return The icon for this app.
     */
    public Drawable getAppIcon(ResolveInfo resolveInfo) {
        return resolveInfo.loadIcon(mPackageManager);
    }
}
