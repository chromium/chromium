// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

/**
 * This class holds the data used to represent a selectable account in the
 * Account Selection sheet.
 */
public class Account {
    private final String mSubject;
    private final String mEmail;
    private final String mName;
    private final String mGivenName;
    private final String mPicture;
    private final String mOriginUrl;

    /**
     * @param subject Subject shown to the user.
     * @param email Email shown to the user.
     * @param givenName Given name.
     * @param picture picture.
     * @param originUrl Origin URL for the IDP.
     */
    public Account(String subject, String email, String name, String givenName, String picture,
            String originUrl) {
        assert subject != null : "Account subject is null!";
        mSubject = subject;
        mEmail = email;
        mName = name;
        mGivenName = givenName;
        mPicture = picture;
        mOriginUrl = originUrl;
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

    public String getPicture() {
        return mPicture;
    }

    public String getOriginUrl() {
        return mOriginUrl;
    }

    public String[] getFields() {
        return new String[] {mSubject, mEmail, mName, mGivenName, mPicture, mOriginUrl};
    }
}
