// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.base.test.util.PackageManagerWrapper;

/**
 * A {@link ContextInterceptor} that rewrites the {@link ApplicationInfo#metaData} surfaced through
 * {@link PackageManager#getApplicationInfo} for the current application. Useful for simulating the
 * presence of application manifest metadata entries.
 */
public final class ApplicationMetaDataInterceptor implements ContextInterceptor {
    /**
     * An interface for providing a replacement for Android {@link ApplicationInfo#metaData}.
     */
    public static interface Replacer {
        /**
         * Provides {@link ApplicationInfo#metaData} that the current application metadata should be
         * replaced with.
         *
         * @param applicationMetaData the original application metadata to be replaced. Note that
         * this parameter can be null since {@link ApplicationInfo#metaData} can be null. This
         * method <b>must not</b> mutate this parameter; a {@link Bundle#Bundle(Bundle) copy} should
         * be returned instead.
         * @return the new application metadata to be returned in {@link ApplicationInfo#metaData}
         */
        @Nullable
        public Bundle replaceApplicationMetaData(@Nullable Bundle applicationMetaData);
    }

    private final Replacer mReplacer;

    public ApplicationMetaDataInterceptor(Replacer replacer) {
        mReplacer = replacer;
    }

    private ApplicationInfo replaceApplicationInfo(ApplicationInfo applicationInfo) {
        Bundle newMetaData = mReplacer.replaceApplicationMetaData(applicationInfo.metaData);
        if (newMetaData != applicationInfo.metaData) {
            applicationInfo = new ApplicationInfo(applicationInfo);
            applicationInfo.metaData = newMetaData;
        }
        return applicationInfo;
    }

    @Override
    public Context interceptContext(Context context) {
        return new ContextWrapper(context) {
            @Override
            public PackageManager getPackageManager() {
                return new PackageManagerWrapper(super.getPackageManager()) {
                    @Override
                    public ApplicationInfo getApplicationInfo(String packageName, int flags)
                            throws PackageManager.NameNotFoundException {
                        ApplicationInfo applicationInfo =
                                super.getApplicationInfo(packageName, flags);
                        if (packageName.equals(getPackageName())
                                && (flags & PackageManager.GET_META_DATA) != 0) {
                            applicationInfo = replaceApplicationInfo(applicationInfo);
                        }
                        return applicationInfo;
                    }
                };
            }
        };
    }
}
