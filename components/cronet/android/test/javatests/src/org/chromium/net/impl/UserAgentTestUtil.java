// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.net.http.HttpEngine;
import android.os.Build;
import android.os.Bundle;

import org.chromium.build.BuildConfig;
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
        // Note we check for BuildConfig.CRONET_FOR_AOSP_BUILD because in AOSP we run tests against
        // both STATICALLY_LINKED (Cronet embedded inside the test suite itself) and AOSP_PLATFORM
        // (tests running against HttpEngine from the device bootclasspath). Cronet AOSP builds will
        // always use the new user agent logic, even when running embedded inside the test suite,
        // so we need to make sure we need to use the new logic when running an AOSP build, even if
        // the test is running in STATICALLY_LINKED mode.
        // TODO(https://crbug.com/460049393): make this less confusing - arguably a cleaner fix
        // would be to make CronetSource return STATICALLY_LINKED instead of PLATFORM in this case,
        // to make the code under test use the old logic.
        return (BuildConfig.CRONET_FOR_AOSP_BUILD
                        || implementationUnderTest == CronetImplementation.AOSP_PLATFORM)
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
        return String.format(
                "AndroidHttpClient (Linux; U; Android %s; %s; %s; Build/%s; Cronet/%s)",
                Build.VERSION.RELEASE,
                Locale.getDefault().toString(),
                Build.MODEL,
                Build.ID,
                getImplVersion(implementationUnderTest));
    }
}
