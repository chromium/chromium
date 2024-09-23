// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;

import org.chromium.url.GURL;

/**
 * This class holds the data used to represent a selectable account in the Account Selection sheet.
 * Android counterpart of IdentityRequestAccount in
 * //content/public/browser/identity_request_account.h
 */
public class Account {
    private final String mId;
    private final String mEmail;
    private final String mName;
    private final String mGivenName;
    private final GURL mPictureUrl;
    private final Bitmap mPictureBitmap;
    private final boolean mIsSignIn;
    private final boolean mIsBrowserTrustedSignIn;

    /**
     * @param id The account ID.
     * @param email Email shown to the user.
     * @param name Full name.
     * @param givenName Given name.
     * @param pictureUrl Picture URL of the avatar shown to the user.
     * @param pictureBitmap The Bitmap for the picture in pictureUrl.
     * @param isSignIn Whether this account's login state is sign in or sign up. Unlike the other
     *     fields this can be populated either by the IDP or by the browser based on its stored
     *     permission grants.
     * @param isBrowserTrustedSignIn Whether this account's login state is sign in or sign up,
     *     trusted by the browser and either observed by the browser or claimed by IDP if the IDP
     *     has third-party cookie access.
     */
    @CalledByNative
    public Account(
            String id,
            String email,
            String name,
            String givenName,
            GURL pictureUrl,
            Bitmap pictureBitmap,
            boolean isSignIn,
            boolean isBrowserTrustedSignIn) {
        mId = id;
        mEmail = email;
        mName = name;
        mGivenName = givenName;
        mPictureUrl = pictureUrl;
        mPictureBitmap = pictureBitmap;
        mIsSignIn = isSignIn;
        mIsBrowserTrustedSignIn = isBrowserTrustedSignIn;
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

    public boolean isBrowserTrustedSignIn() {
        return mIsBrowserTrustedSignIn;
    }

    // Return all the String fields. Note that this excludes non-string fields, in particular
    // mPictureUrl.
    public String[] getStringFields() {
        return new String[] {mId, mEmail, mName, mGivenName};
    }
}
