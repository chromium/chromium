// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.os.Bundle;

import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.net.ContextInterceptor;

/**
 * A {@link ContextInterceptor} that makes the intercepted Context advertise a specific set of
 * Cronet manifest meta-data.
 *
 * @see org.chromium.net.impl.CronetManifest
 */
public final class CronetManifestInterceptor implements ContextInterceptor {
    private final Bundle mMetaData;

    /**
     * @param metaData the meta-data to return in Cronet manifest meta-data queries on intercepted
     * Contexts.
     */
    public CronetManifestInterceptor(Bundle metaData) {
        mMetaData = metaData;
    }

    @Override
    public Context interceptContext(Context context) {
        return new ContextWrapper(context) {
            @Override
            public PackageManager getPackageManager() {
                return new PackageManagerWrapper(super.getPackageManager()) {
                    @Override
                    public ServiceInfo getServiceInfo(ComponentName componentName, int flags)
                            throws NameNotFoundException {
                        if (!componentName.equals(
                                new ComponentName(
                                        getBaseContext(),
                                        CronetManifest.META_DATA_HOLDER_SERVICE_NAME))) {
                            return super.getServiceInfo(componentName, flags);
                        }

                        ServiceInfo serviceInfo = new ServiceInfo();
                        serviceInfo.metaData = mMetaData;
                        return serviceInfo;
                    }
                };
            }
        };
    }
}
