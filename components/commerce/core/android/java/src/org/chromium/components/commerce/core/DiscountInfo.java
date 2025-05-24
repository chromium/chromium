// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.commerce.core;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Optional;

/** A data container for discount info provided by the shopping service. */
@NullMarked
public final class DiscountInfo {
    @DiscountClusterType public final int clusterType;

    @DiscountType public final int type;

    public final String languageCode;
    public final String descriptionDetail;
    public final Optional<String> termsAndConditions;
    public final String valueInText;
    public final Optional<String> discountCode;
    public final long id;
    public final boolean isMerchantWide;
    public final Optional<Double> expiryTimeSec;
    public final long offerId;

    // Constructor
    @CalledByNative
    public DiscountInfo(
            @DiscountClusterType int clusterType,
            @DiscountType int type,
            String languageCode,
            String descriptionDetail,
            @Nullable String termsAndConditions,
            String valueInText,
            @Nullable String discountCode,
            long id,
            boolean isMerchantWide,
            boolean hasExpiryTime,
            double expiryTimeSec,
            long offerId) {
        this.clusterType = clusterType;
        this.type = type;
        this.languageCode = languageCode;
        this.descriptionDetail = descriptionDetail;
        this.valueInText = valueInText;
        this.termsAndConditions =
                termsAndConditions == null ? Optional.empty() : Optional.of(termsAndConditions);
        this.discountCode = discountCode == null ? Optional.empty() : Optional.of(discountCode);
        this.id = id;
        this.isMerchantWide = isMerchantWide;
        this.expiryTimeSec = !hasExpiryTime ? Optional.empty() : Optional.of(expiryTimeSec);
        this.offerId = offerId;
    }
}
