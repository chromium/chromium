// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.url.GURL;

/**
 * This class holds the data used to represent a selectable account in the
 * Account Selection sheet.
 */
public class Account {
    private final String mSubject;
    private final String mEmail;
    private final String mName;
    private final String mGivenName;
    private final GURL mPictureUrl;
    private final String[] mLoginHints;
    private final boolean mIsSignIn;

    /**
     * @param subject Subject shown to the user.
     * @param email Email shown to the user.
     * @param givenName Given name.
     * @param picture picture URL of the avatar shown to the user.
     * @param loginHints the login hints which can match to this account.
     * @param isSignIn whether this account is a sign in or a sign up.
     */
    @CalledByNative
    public Account(String subject, String email, String name, String givenName, GURL pictureUrl,
            String[] loginHints, boolean isSignIn) {
        assert subject != null : "Account subject is null!";
        mSubject = subject;
        mEmail = email;
        mName = name;
        mGivenName = givenName;
        mPictureUrl = pictureUrl;
        mLoginHints = loginHints;
        mIsSignIn = isSignIn;
    }

    public String getSubject() {
        return mSubject;
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

    public String[] getLoginHints() {
        return mLoginHints;
    }

    public boolean isSignIn() {
        return mIsSignIn;
    }

    // Return all the String fields. Note that this excludes non-string fields, in particular
    // mPictureUrl and mLoginHints.
    public String[] getStringFields() {
        return new String[] {mSubject, mEmail, mName, mGivenName};
    }
}
