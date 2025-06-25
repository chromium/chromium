// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.webid.IdentityRequestDialogDisclosureField;

/**
 * This class holds the data used to represent a selectable account in the Account Selection sheet.
 * Android counterpart of IdentityRequestAccount in
 * //content/public/browser/identity_request_account.h
 */
@NullMarked
public class Account {
    private final String mId;
    private final String mDisplayIdentifier;
    private final String mDisplayName;
    private final String mGivenName;
    // The secondary description. This value is not null if and only if the UI being displayed is
    // multi IDP. The text contains the IDP origin and possibly the last used timestamp if this is
    // an account that has been used in the device before.
    private final @Nullable String mSecondaryDescription;
    private final Bitmap mPictureBitmap;
    private final Bitmap mCircledBadgedPictureBitmap;
    private final boolean mIsIdpClaimedSignIn;
    private final boolean mIsBrowserTrustedSignIn;
    private final boolean mIsFilteredOut;
    private final @IdentityRequestDialogDisclosureField int[] mFields;
    private final IdentityProviderData mIdentityProviderData;

    /**
     * @param id The account ID.
     * @param displayIdentifier Identifier to show for the user (e.g. email).
     * @param displayName Name to show for the user.
     * @param givenName Given name.
     * @param secondaryDescription The secondary description for the account button. This is only
     *     used when multiple IDPs are being used in the dialog.
     * @param pictureBitmap The Bitmap for the picture.
     * @param circledBadgedPictureBitmap The Bitmap for the circled and badged picture. This is only
     *     used when multiple IDPs are being used in the dialog.
     * @param isIdpClaimedSignIn Whether this account's login state is sign in or sign up, populated
     *     by the IDP through an approved clients list.
     * @param isBrowserTrustedSignIn Whether this account's login state is sign in or sign up,
     *     trusted by the browser and either observed by the browser through stored permission
     *     grants or claimed by IDP if the IDP has third-party cookie access.
     * @param isFilteredOut Whether this account is filtered out or not. If true, the account must
     *     be shown disabled since it cannot be used by the user.
     * @param identityProviderData The IdentityProviderData corresponding to the IDP to which this
     *     account belongs to.
     */
    @CalledByNative
    public Account(
            @JniType("std::string") String id,
            @JniType("std::string") String displayIdentifier,
            @JniType("std::string") String displayName,
            @JniType("std::string") String givenName,
            @JniType("std::optional<std::string>") @Nullable String secondaryDescription,
            Bitmap pictureBitmap,
            Bitmap circledBadgedPictureBitmap,
            boolean isIdpClaimedSignIn,
            boolean isBrowserTrustedSignIn,
            boolean isFilteredOut,
            @IdentityRequestDialogDisclosureField int[] fields,
            IdentityProviderData identityProviderData) {
        mId = id;
        mDisplayIdentifier = displayIdentifier;
        mDisplayName = displayName;
        mGivenName = givenName;
        mSecondaryDescription = secondaryDescription;
        mPictureBitmap = pictureBitmap;
        mCircledBadgedPictureBitmap = circledBadgedPictureBitmap;
        mIsIdpClaimedSignIn = isIdpClaimedSignIn;
        mIsBrowserTrustedSignIn = isBrowserTrustedSignIn;
        mIsFilteredOut = isFilteredOut;
        mFields = fields;
        mIdentityProviderData = identityProviderData;
    }

    public String getId() {
        return mId;
    }

    public String getDisplayIdentifier() {
        return mDisplayIdentifier;
    }

    public String getDisplayName() {
        return mDisplayName;
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

    public Bitmap getCircledBadgedPictureBitmap() {
        return mCircledBadgedPictureBitmap;
    }

    public boolean isIdpClaimedSignIn() {
        return mIsIdpClaimedSignIn;
    }

    public boolean isBrowserTrustedSignIn() {
        return mIsBrowserTrustedSignIn;
    }

    public boolean isFilteredOut() {
        return mIsFilteredOut;
    }

    public @IdentityRequestDialogDisclosureField int[] getFields() {
        return mFields;
    }

    public IdentityProviderData getIdentityProviderData() {
        return mIdentityProviderData;
    }
}
