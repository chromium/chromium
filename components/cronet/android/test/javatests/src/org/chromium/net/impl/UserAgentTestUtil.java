// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.net.http.HttpEngine;
import android.os.Build;
import android.os.Bundle;

import org.chromium.net.ContextInterceptor;
import org.chromium.net.CronetTestFramework.CronetImplementation;

import java.util.Locale;

public class UserAgentTestUtil {

    public static ContextInterceptor getContextInterceptorWithLegacyUserAgent(boolean value) {
        Bundle metaData = new Bundle();
        metaData.putBoolean(CronetManifest.USE_LEGACY_DEFAULT_USER_AGENT, value);
        return new CronetManifestInterceptor(metaData);
    }

    public static String getImplVersion(CronetImplementation implementationUnderTest) {
        return implementationUnderTest == CronetImplementation.AOSP_PLATFORM
                ? HttpEngine.getVersionString()
                : ImplVersion.getCronetVersion();
    }

    /** Use this when you don't plan to use the manifest to override the default user agent. */
    public static String getDefaultUserAgent(CronetImplementation implementationUnderTest) {
        return implementationUnderTest == CronetImplementation.AOSP_PLATFORM
                ? getUserAgentWithAndroidHttpClient(implementationUnderTest)
                : getUserAgentWithPackageName(implementationUnderTest);
    }

    public static String getUserAgentWithPackageName(CronetImplementation implementationUnderTest) {
        return String.format(
                "org.chromium.net.tests/1 (Linux; U; Android %s; %s; %s; Build/%s; Cronet/%s)",
                Build.VERSION.RELEASE,
                Locale.getDefault().toString(),
                Build.MODEL,
                Build.ID,
                getImplVersion(implementationUnderTest));
    }

    public static String getUserAgentWithAndroidHttpClient(
            CronetImplementation implementationUnderTest) {
        if (implementationUnderTest != CronetImplementation.AOSP_PLATFORM) {
            throw new IllegalArgumentException(
                    "getUserAgentWithAndroidHttpClient should only be called with AOSP_PLATFORM,"
                            + " this test is likely misconfigured.");
        }
        return String.format(
                "AndroidHttpClient (Linux; U; Android %s; %s; %s; Build/%s; Cronet/%s)",
                Build.VERSION.RELEASE,
                Locale.getDefault().toString(),
                Build.MODEL,
                Build.ID,
                getImplVersion(implementationUnderTest));
    }
}
