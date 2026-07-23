// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import androidx.browser.customtabs.CustomTabsService;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.url.Origin;

import java.util.List;

/** Resolves a FedCM Origin to a verified Android app package name and bound service name. */
@JNINamespace("content::webid")
@NullMarked
public class VerifiedOriginResolver {
    private static final String TAG = "OriginResolver";
    private static final String FEDCM_BOUND_SERVICE_INTENT_ACTION = "org.w3.FedCM";

    private long mNativeVerifiedOriginResolver;

    private VerifiedOriginResolver(long nativeVerifiedOriginResolver) {
        mNativeVerifiedOriginResolver = nativeVerifiedOriginResolver;
    }

    @CalledByNative
    private static VerifiedOriginResolver create(long nativeVerifiedOriginResolver) {
        return new VerifiedOriginResolver(nativeVerifiedOriginResolver);
    }

    @CalledByNative
    private void destroy() {
        mNativeVerifiedOriginResolver = 0;
    }

    @CalledByNative
    private void resolve(@JniType("url::Origin") Origin origin) {
        org.chromium.components.embedder_support.util.Origin dalOrigin =
                org.chromium.components.embedder_support.util.Origin.create(origin.toString());
        assert dalOrigin != null;

        Intent intent = new Intent(FEDCM_BOUND_SERVICE_INTENT_ACTION);
        Context context = ContextUtils.getApplicationContext();
        PackageManager packageManager = context.getPackageManager();
        List<ResolveInfo> services = packageManager.queryIntentServices(intent, 0);

        Log.d(TAG, "Discovered FedCM services: " + services);

        if (services.isEmpty()) {
            notifyResolved("", "");
            return;
        }

        List<String> packages = new java.util.ArrayList<>();
        for (ResolveInfo info : services) {
            packages.add(info.serviceInfo.packageName);
        }

        DigitalAssetLinksVerifier.checkPackages(
                packages,
                dalOrigin,
                index -> {
                    if (index != -1) {
                        ResolveInfo info = services.get(index);
                        notifyResolved(info.serviceInfo.packageName, info.serviceInfo.name);
                    } else {
                        Log.d(TAG, "No verified service found for origin");
                        notifyResolved("", "");
                    }
                });
    }

    private void notifyResolved(String packageName, String serviceName) {
        if (mNativeVerifiedOriginResolver != 0) {
            VerifiedOriginResolverJni.get()
                    .onOriginResolved(mNativeVerifiedOriginResolver, packageName, serviceName);
        }
    }

    @CalledByNativeForTesting
    private static void addVerificationOverrideForTesting(
            @JniType("std::string") String packageName, @JniType("url::Origin") Origin origin) {
        org.chromium.components.embedder_support.util.Origin dalOrigin =
                org.chromium.components.embedder_support.util.Origin.create(origin.toString());
        assert dalOrigin != null;
        ChromeOriginVerifier.addVerificationOverride(
                packageName, dalOrigin, CustomTabsService.RELATION_USE_AS_ORIGIN);
    }

    @NativeMethods
    interface Natives {
        void onOriginResolved(
                long nativeVerifiedOriginResolver,
                @JniType("std::string") String packageName,
                @JniType("std::string") String serviceName);
    }
}
