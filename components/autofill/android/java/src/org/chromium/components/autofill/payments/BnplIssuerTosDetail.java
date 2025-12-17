// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

import java.util.List;

/** Detailed BNPL issuer ToS to show in the BNPL issuer ToS bottonsheet. */
@NullMarked
public class BnplIssuerTosDetail {
    /** Issuer that the ToS screen is being shown for. */
    private final String mIssuerId;

    /** True if the issuer is a linked issuer. */
    private final boolean mIsLinkedIssuer;

    /** Display name for the issuer. */
    private final String mIssuerName;

    /** List of legal messages. */
    private final List<LegalMessageLine> mLegalMessageLines;

    /**
     * Creates a new instance of the detailed card information.
     *
     * @param issuerId Issuer that the ToS screen is being shown for.
     * @param isLinkedIssuer True if the issuer is a linked issuer.
     * @param issuerName The display name for the issuer.
     * @param legalMessageLines List of legal messages.
     */
    @CalledByNative
    public BnplIssuerTosDetail(
            @JniType("std::string") String issuerId,
            boolean isLinkedIssuer,
            @JniType("std::u16string") String issuerName,
            @JniType("std::vector") List<LegalMessageLine> legalMessageLines) {
        mIssuerId = issuerId;
        mIsLinkedIssuer = isLinkedIssuer;
        mIssuerName = issuerName;
        mLegalMessageLines = legalMessageLines;
    }

    public String getIssuerId() {
        return mIssuerId;
    }

    public boolean getIsLinkedIssuer() {
        return mIsLinkedIssuer;
    }

    public String getIssuerName() {
        return mIssuerName;
    }

    public List<LegalMessageLine> getLegalMessageLines() {
        return mLegalMessageLines;
    }
}
