// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;

import org.chromium.url.GURL;

/**
 * This class holds the data used to represent a selectable account in the Account Selection sheet.
 */
public class Account {
    private final String mId;
    private final String mEmail;
    private final String mName;
    private final String mGivenName;
    private final GURL mPictureUrl;
    private final Bitmap mPictureBitmap;
    private final boolean mIsSignIn;

    /**
     * @param id The account ID.
     * @param email Email shown to the user.
     * @param name Full name.
     * @param givenName Given name.
     * @param pictureUrl picture URL of the avatar shown to the user.
     * @param pictureBitmap the Bitmap for the picture in pictureUrl.
     * @param isSignIn whether this account is a sign in or a sign up.
     */
    @CalledByNative
    public Account(
            String id,
            String email,
            String name,
            String givenName,
            GURL pictureUrl,
            Bitmap pictureBitmap,
            boolean isSignIn) {
        mId = id;
        mEmail = email;
        mName = name;
        mGivenName = givenName;
        mPictureUrl = pictureUrl;
        mPictureBitmap = pictureBitmap;
        mIsSignIn = isSignIn;
    }

    public String getEmail() {
        return mEmail;
    }

    public String getName() {
        return mName;
    }

    public String getGivenName() {
        return mGivenName;
    }

    public GURL getPictureUrl() {
        return mPictureUrl;
    }

    public Bitmap getPictureBitmap() {
        return mPictureBitmap;
    }

    public boolean isSignIn() {
        return mIsSignIn;
    }

    // Return all the String fields. Note that this excludes non-string fields, in particular
    // mPictureUrl.
    public String[] getStringFields() {
        return new String[] {mId, mEmail, mName, mGivenName};
    }
}
