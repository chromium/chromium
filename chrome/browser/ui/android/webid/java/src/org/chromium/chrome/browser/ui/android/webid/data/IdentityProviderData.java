// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid.data;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.blink.mojom.RpContext;
import org.chromium.content.webid.IdentityRequestDialogDisclosureField;

/**
 * Holds data associated with the identity provider in FedCM dialogs. Android counterpart of
 * IdentityProviderData in //content/public/browser/identity_request_dialog_controller.h
 */
public class IdentityProviderData {
    private final String mIdpForDisplay;
    private final IdentityProviderMetadata mIdpMetadata;
    private final ClientIdMetadata mClientMetadata;
    private @RpContext.EnumType int mRpContext;
    private @IdentityRequestDialogDisclosureField int[] mDisclosureFields;
    private final boolean mHasLoginStatusMismatch;

    @CalledByNative
    public IdentityProviderData(
            @JniType("std::string") String idpForDisplay,
            IdentityProviderMetadata idpMetadata,
            ClientIdMetadata clientMetadata,
            @RpContext.EnumType int rpContext,
            @IdentityRequestDialogDisclosureField int[] disclosureFields,
            boolean hasLoginStatusMismatch) {
        mIdpForDisplay = idpForDisplay;
        mIdpMetadata = idpMetadata;
        mClientMetadata = clientMetadata;
        mRpContext = rpContext;
        mDisclosureFields = disclosureFields;
        mHasLoginStatusMismatch = hasLoginStatusMismatch;
    }

    public String getIdpForDisplay() {
        return mIdpForDisplay;
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

    public @IdentityRequestDialogDisclosureField int[] getDisclosureFields() {
        return mDisclosureFields;
    }

    public boolean getHasLoginStatusMismatch() {
        return mHasLoginStatusMismatch;
    }

    @VisibleForTesting
    public void setDisclosureFields(@IdentityRequestDialogDisclosureField int[] disclosureFields) {
        mDisclosureFields = disclosureFields;
    }

    @VisibleForTesting
    public void setRpContext(@RpContext.EnumType int rpContext) {
        mRpContext = rpContext;
    }
}
