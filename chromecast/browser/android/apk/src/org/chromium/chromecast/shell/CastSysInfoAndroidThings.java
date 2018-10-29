// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import com.google.android.things.AndroidThings;
import com.google.android.things.factory.FactoryDataManager;
import com.google.android.things.update.UpdateManager;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;

/**
 * Java implementation of CastSysInfoAndroidThings methods.
 */
@JNINamespace("chromecast")
public final class CastSysInfoAndroidThings {
    private static final String TAG = "CastSysInfoAndroidThings";
    private static final String FACTORY_LOCALE_LIST_FILE = "locale_list.txt";

    @CalledByNative
    private static String getProductName() {
        return AndroidThings.Product.NAME;
    }

    @CalledByNative
    private static String getDeviceModel() {
        return AndroidThings.Product.MODEL;
    }

    @CalledByNative
    private static String getManufacturer() {
        return AndroidThings.Product.MANUFACTURER;
    }

    @CalledByNative
    private static String getReleaseChannel() {
        return UpdateManager.getInstance().getChannel();
    }

    @CalledByNative
    private static String[] getFactoryLocaleList() {
        ArrayList<String> locale_list = new ArrayList<String>();
        try {
            FactoryDataManager factoryManager = FactoryDataManager.getInstance();
            try (InputStream input = factoryManager.openFile(FACTORY_LOCALE_LIST_FILE);
                    BufferedReader reader = new BufferedReader(new InputStreamReader(input))) {
                for (String line = reader.readLine(); line != null; line = reader.readLine()) {
                    locale_list.add(line);
                }
            }
        } catch (IllegalStateException e) {
            Log.e(TAG, "Failed to connect to FactoryDataService", e);
        } catch (IllegalArgumentException | IOException e) {
            Log.w(TAG, "Factory file %s doesn't exist or can't be opened.",
                    FACTORY_LOCALE_LIST_FILE, e);
        }
        return locale_list.toArray(new String[locale_list.size()]);
    }
}
