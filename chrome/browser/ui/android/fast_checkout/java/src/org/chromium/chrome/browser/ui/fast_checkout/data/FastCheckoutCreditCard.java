// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.data;

import android.content.Context;
import android.util.ArrayMap;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.fast_checkout.R;
import org.chromium.chrome.browser.ui.suggestion.Icon;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.url.GURL;

import java.util.Map;

/** A credit card, similar to the one used by the PersonalDataManager. */
public class FastCheckoutCreditCard {
    // Mappings from name: chrome/browser/ui/autofill/autofill_popup_controller_utils.cc
    // Mappings to resource: chrome/browser/android/resource_id.h
    private static final Map<Integer, Integer> sResourceMap;
    private static final Map<Integer, Integer> sResourceMetadataMap;

    static {
        Map<Integer, Integer> map = new ArrayMap<>();
        Map<Integer, Integer> metadataMap = new ArrayMap<>();

        map.put(Icon.CARD_AMERICAN_EXPRESS, R.drawable.amex_card);
        map.put(Icon.CARD_DINERS, R.drawable.diners_card);
        map.put(Icon.CARD_DISCOVER, R.drawable.discover_card);
        map.put(Icon.CARD_ELO, R.drawable.elo_card);
        map.put(Icon.CARD_GENERIC, R.drawable.ic_credit_card_black);
        map.put(Icon.CARD_JCB, R.drawable.jcb_card);
        map.put(Icon.CARD_MASTER_CARD, R.drawable.mc_card);
        map.put(Icon.CARD_MIR, R.drawable.mir_card);
        map.put(Icon.CARD_TROY, R.drawable.troy_card);
        map.put(Icon.CARD_UNION_PAY, R.drawable.unionpay_card);
        map.put(Icon.CARD_VERVE, R.drawable.verve_card);
        map.put(Icon.CARD_VISA, R.drawable.visa_card);
        map.put(Icon.GOOGLE_PAY, R.drawable.google_pay);

        metadataMap.put(Icon.CARD_AMERICAN_EXPRESS, R.drawable.amex_metadata_card);
        metadataMap.put(Icon.CARD_DINERS, R.drawable.diners_metadata_card);
        metadataMap.put(Icon.CARD_DISCOVER, R.drawable.discover_metadata_card);
        metadataMap.put(Icon.CARD_ELO, R.drawable.elo_metadata_card);
        metadataMap.put(Icon.CARD_GENERIC, R.drawable.ic_metadata_credit_card);
        metadataMap.put(Icon.CARD_JCB, R.drawable.jcb_metadata_card);
        metadataMap.put(Icon.CARD_MASTER_CARD, R.drawable.mc_metadata_card);
        metadataMap.put(Icon.CARD_MIR, R.drawable.mir_metadata_card);
        metadataMap.put(Icon.CARD_TROY, R.drawable.troy_metadata_card);
        metadataMap.put(Icon.CARD_UNION_PAY, R.drawable.unionpay_metadata_card);
        metadataMap.put(Icon.CARD_VERVE, R.drawable.verve_metadata_card);
        metadataMap.put(Icon.CARD_VISA, R.drawable.visa_metadata_card);
        metadataMap.put(Icon.GOOGLE_PAY, R.drawable.google_pay);

        sResourceMap = map;
        sResourceMetadataMap = metadataMap;
    }

    private final String mGUID;
    private final String mOrigin;
    private final boolean mIsLocal;
    private final String mName;
    private final String mNumber;
    private final String mObfuscatedNumber;
    private final String mMonth;
    private final String mYear;
    private final String mBasicCardIssuerNetwork;
    private final @Icon int mIssuerIcon;
    private final String mBillingAddressId;
    private final String mServerId;
    private final long mInstrumentId;
    private final String mNickname;
    private final GURL mCardArtUrl;
    private final @VirtualCardEnrollmentState int mVirtualCardEnrollmentState;
    private final String mProductDescription;

    @CalledByNative
    public FastCheckoutCreditCard(
            String guid,
            String origin,
            boolean isLocal,
            String name,
            String number,
            String obfuscatedNumber,
            String month,
            String year,
            String basicCardIssuerNetwork,
            @Icon int issuerIcon,
            String billingAddressId,
            String serverId,
            long instrumentId,
            String nickname,
            GURL cardArtUrl,
            @VirtualCardEnrollmentState int virtualCardEnrollmentState,
            String productDescription) {
        mGUID = guid;
        mOrigin = origin;
        mIsLocal = isLocal;
        mName = name;
        mNumber = number;
        mObfuscatedNumber = obfuscatedNumber;
        mMonth = month;
        mYear = year;
        mBasicCardIssuerNetwork = basicCardIssuerNetwork;
        mIssuerIcon = issuerIcon;
        mBillingAddressId = billingAddressId;
        mServerId = serverId;
        mInstrumentId = instrumentId;
        mNickname = nickname;
        mCardArtUrl = cardArtUrl;
        mVirtualCardEnrollmentState = virtualCardEnrollmentState;
        mProductDescription = productDescription;
    }

    @CalledByNative
    public String getGUID() {
        return mGUID;
    }

    @CalledByNative
    public String getOrigin() {
        return mOrigin;
    }

    @CalledByNative
    public boolean getIsLocal() {
        return mIsLocal;
    }

    @CalledByNative
    public String getName() {
        return mName;
    }

    @CalledByNative
    public String getNumber() {
        return mNumber;
    }

    public String getObfuscatedNumber() {
        return mObfuscatedNumber;
    }

    @CalledByNative
    public String getMonth() {
        return mMonth;
    }

    @CalledByNative
    public String getYear() {
        return mYear;
    }

    @CalledByNative
    public String getBasicCardIssuerNetwork() {
        return mBasicCardIssuerNetwork;
    }

    public @Icon int getIssuerIcon() {
        return mIssuerIcon;
    }

    @CalledByNative
    public String getBillingAddressId() {
        return mBillingAddressId;
    }

    @CalledByNative
    public String getServerId() {
        return mServerId;
    }

    @CalledByNative
    public long getInstrumentId() {
        return mInstrumentId;
    }

    @CalledByNative
    public String getNickname() {
        return mNickname;
    }

    @CalledByNative
    public GURL getCardArtUrl() {
        return mCardArtUrl;
    }

    @CalledByNative
    public @VirtualCardEnrollmentState int getVirtualCardEnrollmentState() {
        return mVirtualCardEnrollmentState;
    }

    @CalledByNative
    public String getProductDescription() {
        return mProductDescription;
    }

    public String getFormattedExpirationDate(Context context) {
        return getMonth()
                + context.getResources().getString(R.string.autofill_expiration_date_separator)
                + getYear();
    }

    public int getIssuerIconDrawableId() {
        @Icon int issuerIconDrawable = getIssuerIcon();
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES)) {
            if (sResourceMetadataMap.containsKey(issuerIconDrawable)) {
                if (issuerIconDrawable != Icon.CARD_VERVE
                        || ChromeFeatureList.isEnabled(
                                ChromeFeatureList.AUTOFILL_ENABLE_VERVE_CARD_SUPPORT)) {
                    return sResourceMetadataMap.get(issuerIconDrawable);
                }
            }
        } else {
            if (sResourceMap.containsKey(issuerIconDrawable)) {
                if (issuerIconDrawable != Icon.CARD_VERVE
                        || ChromeFeatureList.isEnabled(
                                ChromeFeatureList.AUTOFILL_ENABLE_VERVE_CARD_SUPPORT)) {
                    return sResourceMap.get(issuerIconDrawable);
                }
            }
        }
        return R.drawable.ic_credit_card_black;
    }
}
