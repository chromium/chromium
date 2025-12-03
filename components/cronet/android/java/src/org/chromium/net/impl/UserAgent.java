// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;

import org.chromium.net.impl.CronetLogger.CronetSource;

import java.util.Locale;

/** Constructs a User-Agent string. */
public final class UserAgent {
    private static final Object sLock = new Object();

    private static final int VERSION_CODE_UNINITIALIZED = 0;
    private static int sVersionCode = VERSION_CODE_UNINITIALIZED;

    private UserAgent() {}

    /**
     * Constructs a User-Agent string including application name and version, system build version,
     * model and Id, and Cronet version.
     *
     * @param context the context to fetch the application name and version from.
     * @param source type of the Cronet build.
     * @param version the version of the Cronet build.
     * @return User-Agent string.
     */
    public static String from(Context context, CronetSource source, String version) {
        StringBuilder builder = new StringBuilder();

        if (source == CronetSource.CRONET_SOURCE_PLATFORM
                && !CronetManifest.shouldUseLegacyDefaultUserAgent(context)) {
            builder.append("AndroidHttpClient");
        } else {
            // Our package name and version.
            builder.append(context.getPackageName());
            builder.append('/');
            builder.append(versionFromContext(context));
        }

        // The platform version.
        builder.append(" (Linux; U; Android ");
        builder.append(Build.VERSION.RELEASE);
        builder.append("; ");
        builder.append(Locale.getDefault().toString());

        String model = Build.MODEL;
        if (model.length() > 0) {
            builder.append("; ");
            builder.append(model);
        }

        String id = Build.ID;
        if (id.length() > 0) {
            builder.append("; Build/");
            builder.append(id);
        }

        builder.append(";");
        appendCronetVersion(builder, version);

        builder.append(')');

        return builder.toString();
    }

    /**
     * Constructs default QUIC User Agent Id string including application name and Cronet version.
     *
     * @param context the context to fetch the application name from.
     * @return User-Agent string.
     */
    static String getQuicUserAgentIdFrom(Context context, String version) {
        StringBuilder builder = new StringBuilder();

        // Application name and cronet version.
        builder.append(context.getPackageName());
        appendCronetVersion(builder, version);

        return builder.toString();
    }

    private static int versionFromContext(Context context) {
        synchronized (sLock) {
            if (sVersionCode == VERSION_CODE_UNINITIALIZED) {
                PackageManager packageManager = context.getPackageManager();
                String packageName = context.getPackageName();
                try {
                    PackageInfo packageInfo = packageManager.getPackageInfo(packageName, 0);
                    sVersionCode = packageInfo.versionCode;
                } catch (NameNotFoundException e) {
                    throw new IllegalStateException("Cannot determine package version");
                }
            }
            return sVersionCode;
        }
    }

    private static void appendCronetVersion(StringBuilder builder, String version) {
        builder.append(" Cronet/");
        builder.append(version);
    }
}
