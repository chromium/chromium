// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.Nullable;

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
    // The secondary description. This value is not null if and only if the UI being displayed is
    // multi IDP. The text contains the IDP origin and possibly the last used timestamp if this is
    // an account that has been used in the device before.
    private final @Nullable String mSecondaryDescription;
    private final Bitmap mPictureBitmap;
    private final boolean mIsSignIn;
    private final boolean mIsBrowserTrustedSignIn;
    private final boolean mIsFilteredOut;
    private final IdentityProviderData mIdentityProviderData;

    /**
     * @param id The account ID.
     * @param email Email shown to the user.
     * @param name Full name.
     * @param givenName Given name.
     * @param pictureBitmap The Bitmap for the picture.
     * @param isSignIn Whether this account's login state is sign in or sign up. Unlike the other
     *     fields this can be populated either by the IDP or by the browser based on its stored
     *     permission grants.
     * @param isBrowserTrustedSignIn Whether this account's login state is sign in or sign up,
     *     trusted by the browser and either observed by the browser or claimed by IDP if the IDP
     *     has third-party cookie access.
     * @param isFilteredOut Whether this account is filtered out or not. If true, the account must
     *     be shown disabled since it cannot be used by the user.
     * @param identityProviderData The IdentityProviderData corresponding to the IDP to which this
     *     account belongs to.
     */
    @CalledByNative
    public Account(
            @JniType("std::string") String id,
            @JniType("std::string") String email,
            @JniType("std::string") String name,
            @JniType("std::string") String givenName,
            @JniType("std::optional<std::string>") @Nullable String secondaryDescription,
            Bitmap pictureBitmap,
            boolean isSignIn,
            boolean isBrowserTrustedSignIn,
            boolean isFilteredOut,
            IdentityProviderData identityProviderData) {
        mId = id;
        mEmail = email;
        mName = name;
        mGivenName = givenName;
        mSecondaryDescription = secondaryDescription;
        mPictureBitmap = pictureBitmap;
        mIsSignIn = isSignIn;
        mIsBrowserTrustedSignIn = isBrowserTrustedSignIn;
        mIsFilteredOut = isFilteredOut;
        mIdentityProviderData = identityProviderData;
    }

    public String getId() {
        return mId;
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

    public @Nullable String getSecondaryDescription() {
        return mSecondaryDescription;
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

    public boolean isFilteredOut() {
        return mIsFilteredOut;
    }

    public IdentityProviderData getIdentityProviderData() {
        return mIdentityProviderData;
    }
}
