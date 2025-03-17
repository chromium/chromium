// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.util.Log;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Formatter;

/** Helper functions for working with hashes. */
@NullMarked
public final class HashUtil {
    private static final String TAG = "HashUtil";

    private HashUtil() {}

    public static class Params {
        private final String mText;
        private @Nullable String mSalt;

        public Params(String text) {
            mText = text;
        }

        public Params withSalt(@Nullable String salt) {
            mSalt = salt;
            return this;
        }
    }

    public static @Nullable String getMd5Hash(Params params) {
        return getHash(params, "MD5");
    }

    private static @Nullable String getHash(Params params, String algorithm) {
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
