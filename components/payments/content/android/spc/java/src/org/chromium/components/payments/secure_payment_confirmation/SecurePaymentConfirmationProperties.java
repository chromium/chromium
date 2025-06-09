// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import android.graphics.drawable.Drawable;
import android.text.SpannableString;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

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

    /**
     * When true, shows the issuer and network icons (if provided) in the header instead of the
     * default header image.
     */
    /* package */ static final ReadableBooleanPropertyKey SHOWS_ISSUER_NETWORK_ICONS =
            new ReadableBooleanPropertyKey();

    /** The issuer icon for the UI. */
    /* package */ static final ReadableObjectPropertyKey<Drawable> ISSUER_ICON =
            new ReadableObjectPropertyKey<>();

    /** The network icon for the UI. */
    /* package */ static final ReadableObjectPropertyKey<Drawable> NETWORK_ICON =
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
                SHOWS_ISSUER_NETWORK_ICONS,
                ISSUER_ICON,
                NETWORK_ICON,
                TITLE,
                ITEM_LIST_ADAPTER,
                OPT_OUT_TEXT,
                FOOTNOTE,
                CONTINUE_BUTTON_LABEL,
            };

    /** Do not instantiate. */
    private SecurePaymentConfirmationProperties() {}
}
