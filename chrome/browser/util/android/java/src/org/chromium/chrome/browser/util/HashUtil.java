// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.util.Log;

import org.chromium.base.ApiCompatibilityUtils;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Formatter;

/**
 * Helper functions for working with hashes.
 */
public final class HashUtil {
    private static final String TAG = "HashUtil";

    private HashUtil() {}

    public static class Params {
        private final String mText;
        private String mSalt;

        public Params(String text) {
            mText = text;
        }

        public Params withSalt(String salt) {
            mSalt = salt;
            return this;
        }
    }

    public static String getMd5Hash(Params params) {
        return getHash(params, "MD5");
    }

    private static String getHash(Params params, String algorithm) {
        try {
            String digestText = params.mText + (params.mSalt == null ? "" : params.mSalt);
            MessageDigest m = MessageDigest.getInstance(algorithm);
            byte[] digest = m.digest(ApiCompatibilityUtils.getBytesUtf8(digestText));
            return encodeHex(digest);
        } catch (NoSuchAlgorithmException e) {
            Log.e(TAG, "Unable to find digest algorithm " + algorithm);
            return null;
        }
    }

    private static String encodeHex(byte[] data) {
        StringBuilder sb = new StringBuilder(data.length * 2);
        Formatter formatter = new Formatter(sb);
        for (byte b : data) {
            formatter.format("%02x", b);
        }
        return sb.toString();
    }
}
