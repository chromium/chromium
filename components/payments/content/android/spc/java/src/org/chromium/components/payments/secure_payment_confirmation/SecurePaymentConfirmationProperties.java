// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.graphics.drawable.Drawable;
import android.text.SpannableString;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.payments.PaymentApp.PaymentEntityLogo;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;

/** The properties of the SecurePaymentConfirmation UI, which fully describe the state of the UI. */
@NullMarked
/* package */ class SecurePaymentConfirmationProperties {
    /** The properties of the item used to populate the SPC item list. */
    public static class ItemProperties {
        /** The icon for the UI. */
        /* package */ static final ReadableObjectPropertyKey<Drawable> ICON =
                new ReadableObjectPropertyKey<>();

        /** The label of the icon used for a11y announcements. */
        /* package */ static final ReadableObjectPropertyKey<String> ICON_LABEL =
                new ReadableObjectPropertyKey<>();

        /** The primary text for the UI. */
        /* package */ static final ReadableObjectPropertyKey<String> PRIMARY_TEXT =
                new ReadableObjectPropertyKey<>();

        /** The secondary text for the UI. */
        /* package */ static final ReadableObjectPropertyKey<String> SECONDARY_TEXT =
                new ReadableObjectPropertyKey<>();

        /* package */ static final PropertyKey[] ALL_KEYS =
                new PropertyKey[] {ICON, ICON_LABEL, PRIMARY_TEXT, SECONDARY_TEXT};

        /** Do not instantiate. */
        private ItemProperties() {}
    }

    /** The list of header logos for the UI. */
    /* package */ static final ReadableObjectPropertyKey<List<PaymentEntityLogo>> HEADER_LOGOS =
            new ReadableObjectPropertyKey<>();

    /** The title text for the UI. */
    /* package */ static final ReadableObjectPropertyKey<String> TITLE =
            new ReadableObjectPropertyKey<>();

    /**
     * The item list recycler view adapter for the SPC view. Contains the Store, Payment, and Total
     * rows.
     */
    /* package */ static final ReadableObjectPropertyKey<SimpleRecyclerViewAdapter>
            ITEM_LIST_ADAPTER = new ReadableObjectPropertyKey<>();

    /** The opt out text for the UI. If empty, the UI will be hidden. */
    /* package */ static final ReadableObjectPropertyKey<SpannableString> OPT_OUT_TEXT =
            new ReadableObjectPropertyKey<>();

    /** The footnote for the UI. */
    /* package */ static final ReadableObjectPropertyKey<SpannableString> FOOTNOTE =
            new ReadableObjectPropertyKey<>();

    /** The label for the continue button. */
    /* package */ static final ReadableObjectPropertyKey<String> CONTINUE_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>();

    /* package */ static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                HEADER_LOGOS,
                TITLE,
                ITEM_LIST_ADAPTER,
                OPT_OUT_TEXT,
                FOOTNOTE,
                CONTINUE_BUTTON_LABEL,
            };

    /** Do not instantiate. */
    private SecurePaymentConfirmationProperties() {}
}
