// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses.utils;

import android.content.Context;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.ui.base.WindowAndroid;

/** A helper class to open plus address management web page. */
public class PlusAddressesHelper {
    private static final String MANAGE_PLUS_ADDRESSES_URL =
            "https://myaccount.google.com/shielded-email";

    @CalledByNative
    public static void openManagePlusAddresses(WindowAndroid window) {
        Context context = window.getActivity().get();

        if (context == null) {
            return;
        }

        CustomTabActivity.showInfoPage(context, MANAGE_PLUS_ADDRESSES_URL);
    }

    private PlusAddressesHelper() {}
}
