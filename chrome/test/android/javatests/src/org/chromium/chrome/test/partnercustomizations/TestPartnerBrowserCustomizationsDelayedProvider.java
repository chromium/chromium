// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.partnercustomizations;

import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;

import java.util.List;
import java.util.concurrent.CountDownLatch;

/**
 * PartnerBrowserCustomizationsProvider example for testing. This adds one second latency for
 * query function.
 * Note: if you move or rename this class, make sure you have also updated AndroidManifest.xml.
 */
public class TestPartnerBrowserCustomizationsDelayedProvider
        extends TestPartnerBrowserCustomizationsProvider {
    private static String sUriPathToDelay;
    private static CountDownLatch sLatch;

    public TestPartnerBrowserCustomizationsDelayedProvider() {
        super();
        mTag = TestPartnerBrowserCustomizationsDelayedProvider.class.getSimpleName();
    }

    public static void unblockQuery() {
        sLatch.countDown();
    }

    private void setUriPathToDelay(String path) {
        sUriPathToDelay = path;
        sLatch = new CountDownLatch(1);
    }

    @Override
    public Bundle call(String method, String arg, Bundle extras) {
        if (TextUtils.equals(method, "setUriPathToDelay")) {
            setUriPathToDelay(arg);
        }
        return super.call(method, arg, extras);
    }

    @Override
    public Cursor query(
            Uri uri,
            String[] projection,
            String selection,
            String[] selectionArgs,
            String sortOrder) {
        try {
            List<String> pathSegments = uri.getPathSegments();
            if (sUriPathToDelay == null
                    || (pathSegments != null
                            && !pathSegments.isEmpty()
                            && TextUtils.equals(pathSegments.get(0), sUriPathToDelay))) {
                sLatch.await();
            }
        } catch (InterruptedException e) {
            assert false;
        }
        return super.query(uri, projection, selection, selectionArgs, sortOrder);
    }
}
