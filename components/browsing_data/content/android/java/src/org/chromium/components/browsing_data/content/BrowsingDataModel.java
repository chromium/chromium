// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browsing_data.content;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.Origin;

import java.util.HashMap;
import java.util.Map;

public class BrowsingDataModel {

    // A pointer to the C++ object for this model.
    private long mNativeBrowsingDataModel;

    /**
     * Creates a `BrowsingDataModel` object.
     *
     * @param nativeBrowsingDataModel The reference to the C++ android model object.
     */
    public BrowsingDataModel(long nativeBrowsingDataModel) {
        mNativeBrowsingDataModel = nativeBrowsingDataModel;
    }

    /**
     * Gets browsing data in a structure of Map<Origin, BrowsingDataInfo> from the built model.
     *
     * @return Map of origin to info.
     */
    public Map<Origin, BrowsingDataInfo> getBrowsingDataInfo(
            BrowserContextHandle browserContext, boolean fetchImportant) {
        Map<Origin, BrowsingDataInfo> map = new HashMap();
        return BrowsingDataModelJni.get()
                .getBrowsingDataInfo(mNativeBrowsingDataModel, browserContext, map, fetchImportant);
    }

    /**
     * Remove browsing data for a host.
     *
     * @param host The host string for which the data will be removed.
     * @param completed Completion callback to be called when removal is completed.
     */
    public void removeBrowsingData(String host, Runnable completed) {
        BrowsingDataModelJni.get().removeBrowsingData(mNativeBrowsingDataModel, host, completed);
    }

    public void destroy() {
        BrowsingDataModelJni.get().destroy(mNativeBrowsingDataModel, BrowsingDataModel.this);
    }

    @CalledByNative
    private static void insertBrowsingDataInfoIntoMap(
            Map<Origin, BrowsingDataInfo> map,
            Origin origin,
            int cookieCount,
            long storageSize,
            boolean importantDomain) {
        map.put(origin, new BrowsingDataInfo(origin, cookieCount, storageSize, importantDomain));
    }

    @NativeMethods
    interface Natives {
        Map<Origin, BrowsingDataInfo> getBrowsingDataInfo(
                long nativeBrowsingDataModelAndroid,
                BrowserContextHandle browserContext,
                Map<Origin, BrowsingDataInfo> map,
                boolean fetchImportant);

        void removeBrowsingData(
                long nativeBrowsingDataModelAndroid, String host, Runnable completed);

        void destroy(long nativeBrowsingDataModelAndroid, BrowsingDataModel caller);
    }
}
