// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import androidx.annotation.DrawableRes;

/** Detailed card information to show in the various Autofill views. */
public class CardDetail {
    /** The identifier of the drawable of the card issuer icon. */
    public @DrawableRes int issuerIconDrawableId;

    /** The label for the card. */
    public String label;

    /** The sub-label for the card. */
    public String subLabel;

    /**
     * Creates a new instance of the detailed card information.
     *
     * @param iconId ID corresponding to the icon that will be shown for this credit card.
     * @param label The credit card label, for example "***1234".
     * @param subLabel The credit card sub-label, for example "Exp: 06/17".
     */
    public CardDetail(@DrawableRes int iconId, String label, String subLabel) {
        this.issuerIconDrawableId = iconId;
        this.label = label;
        this.subLabel = subLabel;
    }
}
