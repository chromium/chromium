// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.blink.mojom.RpContext;

import java.util.Arrays;
import java.util.List;

/**
 * Holds data associated with the identity provider in FedCM dialogs. Android counterpart of
 * IdentityProviderData in //content/public/browser/identity_request_dialog_controller.h
 */
public class IdentityProviderData {
    private final String mIdpForDisplay;
    private final List<Account> mAccounts;
    private final IdentityProviderMetadata mIdpMetadata;
    private final ClientIdMetadata mClientMetadata;
    private final @RpContext.EnumType int mRpContext;
    private boolean mRequestPermission;
    private final boolean mHasLoginStatusMismatch;

    @CalledByNative
    public IdentityProviderData(
            @JniType("std::string") String idpForDisplay,
            Account[] accounts,
            IdentityProviderMetadata idpMetadata,
            ClientIdMetadata clientMetadata,
            @RpContext.EnumType int rpContext,
            boolean requestPermission,
            boolean hasLoginStatusMismatch) {
        mIdpForDisplay = idpForDisplay;
        mAccounts = Arrays.asList(accounts);
        mIdpMetadata = idpMetadata;
        mClientMetadata = clientMetadata;
        mRpContext = rpContext;
        mRequestPermission = requestPermission;
        mHasLoginStatusMismatch = hasLoginStatusMismatch;
    }

    public String getIdpForDisplay() {
        return mIdpForDisplay;
    }

    public List<Account> getAccounts() {
        return mAccounts;
    }

    public IdentityProviderMetadata getIdpMetadata() {
        return mIdpMetadata;
    }

    public ClientIdMetadata getClientMetadata() {
        return mClientMetadata;
    }

    public @RpContext.EnumType int getRpContext() {
        return mRpContext;
    }

    public boolean getRequestPermission() {
        return mRequestPermission;
    }

    public boolean getHasLoginStatusMismatch() {
        return mHasLoginStatusMismatch;
    }

    @VisibleForTesting
    public void setRequestPermission(boolean requestPermission) {
        mRequestPermission = requestPermission;
    }
}
