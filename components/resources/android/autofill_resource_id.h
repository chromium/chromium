// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file maps resource IDs to Android resource IDs.

// Presence of regular include guards is checked by:
// 1. cpplint
// 2. a custom presubmit in src/PRESUBMIT.py
// 3. clang (but it only checks the guard is correct if present)
// Disable the first two with these magic comments:
// NOLINT(build/header_guard)
// no-include-guard-because-multiply-included

// LINK_RESOURCE_ID is used for IDs that come from a .grd file.
#ifndef LINK_RESOURCE_ID
#error "LINK_RESOURCE_ID should be defined before including this file"
#endif
// DECLARE_RESOURCE_ID is used for IDs that don't have .grd entries, and
// are only declared in this file.
#ifndef DECLARE_RESOURCE_ID
#error "DECLARE_RESOURCE_ID should be defined before including this file"
#endif

// Autofill popup and keyboard accessory images.
// We use Android's |VectorDrawableCompat| for the following images that are
// displayed using |DropdownAdapter|.
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_AMEX_OLD, R.drawable.amex_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_AMEX, R.drawable.amex_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_AMEX_NEW, R.drawable.amex_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_DINERS_OLD, R.drawable.diners_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_DINERS, R.drawable.diners_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_DISCOVER_OLD, R.drawable.discover_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_DISCOVER, R.drawable.discover_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_ELO_OLD, R.drawable.elo_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_ELO, R.drawable.elo_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_GENERIC_OLD, R.drawable.ic_credit_card_black)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_GENERIC, R.drawable.ic_credit_card_black)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_GENERIC_PRIMARY_OLD,
                 R.drawable.ic_credit_card_primary)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_GENERIC_PRIMARY,
                 R.drawable.ic_credit_card_primary)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_JCB_OLD, R.drawable.jcb_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_JCB, R.drawable.jcb_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_MASTERCARD_OLD, R.drawable.mc_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_MASTERCARD, R.drawable.mc_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_MIR_OLD, R.drawable.mir_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_MIR, R.drawable.mir_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_TROY_OLD, R.drawable.troy_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_TROY, R.drawable.troy_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_UNIONPAY_OLD, R.drawable.unionpay_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_UNIONPAY, R.drawable.unionpay_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_VERVE_OLD, R.drawable.verve_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_VERVE, R.drawable.verve_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_VISA_OLD, R.drawable.visa_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_VISA, R.drawable.visa_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY, R.drawable.google_pay)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY_PILL, R.drawable.googlepay_pill)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY_PILL_WITH_GRADIENT,
                 R.drawable.googlepay_pill_with_gradient)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_WALLET, R.drawable.googlewallet)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_WALLET_ICON, R.drawable.google_wallet_24dp)
// Note that R.drawable.googlewallet_icon_with_gradient is always present, but
// the icon in branded builds is different from the one in unbranded builds.
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_WALLET_ICON_WITH_GRADIENT,
                 R.drawable.googlewallet_icon_with_gradient)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_BNPL_GENERIC,
                 R.drawable.bnpl_icon_generic)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_BNPL_GENERIC_OLD,
                 R.drawable.bnpl_icon_generic)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_AMEX_OLD,
                 R.drawable.amex_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_AMEX, R.drawable.amex_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_AMEX_NEW, R.drawable.amex_metadata_card_new)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_CAPITALONE,
                 R.drawable.capitalone_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_DINERS_OLD,
                 R.drawable.diners_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_DINERS,
                 R.drawable.diners_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_DISCOVER_OLD,
                 R.drawable.discover_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_DISCOVER,
                 R.drawable.discover_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_ELO_OLD, R.drawable.elo_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_ELO, R.drawable.elo_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_GENERIC_OLD,
                 R.drawable.ic_metadata_credit_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_GENERIC,
                 R.drawable.ic_metadata_credit_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_JCB_OLD, R.drawable.jcb_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_JCB, R.drawable.jcb_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_MASTERCARD_OLD,
                 R.drawable.mc_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_MASTERCARD,
                 R.drawable.mc_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_MIR_OLD, R.drawable.mir_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_MIR, R.drawable.mir_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_TROY_OLD,
                 R.drawable.troy_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_TROY, R.drawable.troy_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_UNIONPAY_OLD,
                 R.drawable.unionpay_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_UNIONPAY,
                 R.drawable.unionpay_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_VERVE_OLD,
                 R.drawable.verve_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_VERVE, R.drawable.verve_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_VISA_OLD,
                 R.drawable.visa_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_VISA, R.drawable.visa_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_IBAN_OLD, R.drawable.iban_icon)
LINK_RESOURCE_ID(IDR_AUTOFILL_IBAN, R.drawable.iban_icon)

// Use DECLARE_RESOURCE_ID here as these resources are used for android only.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_CC_SCAN_NEW,
                    R.drawable.ic_photo_camera_black)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_OFFER_TAG_GREEN,
                    R.drawable.ic_offer_tag)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_ANDROID_MESSAGES,
                    R.drawable.ic_android_messages_icon)

// Home and work icons.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_HOME, R.drawable.ic_home_24dp)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_WORK, R.drawable.work_logo)

// APC password recovery icon
DECLARE_RESOURCE_ID(IDR_ANDROID_PASSWORD_HISTORY, R.drawable.ic_history_24dp)

// Autofill AI icons.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_PASSPORT, R.drawable.passport)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_ID_CARD, R.drawable.id_card)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_FLIGHT, R.drawable.flight)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_TRAVEL_LUGGAGE_AND_BAGS,
                    R.drawable.travel_luggage_and_bags)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_PERSON_CHECK, R.drawable.person_check)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_VEHICLE, R.drawable.directions_car)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_SPARK, R.drawable.spark)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_FLIGHT_SPARK, R.drawable.flight_spark)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_SHOPPING_BAG, R.drawable.shopping_bag)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_SHOPPING_BAG_SPARK,
                    R.drawable.shopping_bag_spark)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_PASSPORT_SPARK,
                    R.drawable.passport_spark)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_SHIPMENT, R.drawable.local_shipping)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_SHIPMENT_SPARK,
                    R.drawable.local_shipping_spark)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_CAR_SPARK, R.drawable.car_spark)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_ID_CARD_SPARK,
                    R.drawable.id_card_spark)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_ID_CARD_2, R.drawable.id_card_2)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_ID_CARD_2_SPARK,
                    R.drawable.id_card_2_spark)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_CARD_GENERIC_SPARK,
                    R.drawable.card_generic_spark)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_CARD_GENERIC_VECTOR,
                    R.drawable.card_generic_vector)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_LOCATION, R.drawable.location)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_LOCATION_SPARK,
                    R.drawable.location_spark)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_TEXT_SPARK,
                    R.drawable.ic_text_analysis_24dp)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_ID_CHROME_PRODUCT,
                    R.drawable.chrome_product)
// Note that R.drawable.google_wallet_24dp is always present, but the icon in
// branded builds is different from the one in unbranded builds.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_WALLET, R.drawable.google_wallet_24dp)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_EMAIL,
                    R.drawable.ic_outline_email_24dp)

// @memory search icon.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_SEARCH_SPARK, R.drawable.search_spark)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_SAD_TAB, R.drawable.autofill_sad_tab)
