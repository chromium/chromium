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
LINK_RESOURCE_ID(IDR_AUTOFILL_AFFIRM_LINKED, R.drawable.affirm_linked)
LINK_RESOURCE_ID(IDR_AUTOFILL_AFFIRM_UNLINKED, R.drawable.affirm_unlinked)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_AMEX_OLD, R.drawable.amex_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_AMEX, R.drawable.amex_card)
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
// TODO(crbug.com/438784697): update the resource id once the internal assets
// access is added.
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY_AFFIRM, R.drawable.bnpl_icon_generic)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY_AFFIRM_DARK,
                 R.drawable.bnpl_icon_generic)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY_AFTERPAY, R.drawable.bnpl_icon_generic)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY_AFTERPAY_DARK,
                 R.drawable.bnpl_icon_generic)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY_KLARNA, R.drawable.bnpl_icon_generic)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY_KLARNA_DARK,
                 R.drawable.bnpl_icon_generic)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY_ZIP, R.drawable.bnpl_icon_generic)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY_ZIP_DARK, R.drawable.bnpl_icon_generic)
LINK_RESOURCE_ID(IDR_AUTOFILL_KLARNA_LINKED, R.drawable.klarna_linked)
LINK_RESOURCE_ID(IDR_AUTOFILL_KLARNA_UNLINKED, R.drawable.klarna_unlinked)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_BNPL_GENERIC,
                 R.drawable.bnpl_icon_generic)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_BNPL_GENERIC_OLD,
                 R.drawable.bnpl_icon_generic)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_AMEX_OLD,
                 R.drawable.amex_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_AMEX, R.drawable.amex_metadata_card)
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
LINK_RESOURCE_ID(IDR_AUTOFILL_ZIP_LINKED, R.drawable.zip_linked)
LINK_RESOURCE_ID(IDR_AUTOFILL_ZIP_UNLINKED, R.drawable.zip_unlinked)

// Use DECLARE_RESOURCE_ID here as these resources are used for android only.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_CC_SCAN_NEW,
                    R.drawable.ic_photo_camera_black)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_OFFER_TAG_GREEN,
                    R.drawable.ic_offer_tag)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_ANDROID_MESSAGES,
                    R.drawable.ic_android_messages_icon)

// Note that R.drawable.plus_addresses_logo is always present, but the icon in
// branded builds is different from the one in unbranded builds,
DECLARE_RESOURCE_ID(IDR_AUTOFILL_PLUS_ADDRESS,
                    R.drawable.ic_plus_addresses_logo_16dp)

// Home and work icons.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_HOME, R.drawable.home_logo)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_WORK, R.drawable.work_logo)

// APC password recovery icon
DECLARE_RESOURCE_ID(IDR_ANDROID_PASSWORD_HISTORY, R.drawable.ic_history_24dp)

// Autofill AI icons.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_ID_CARD, R.drawable.id_card)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_FLIGHT, R.drawable.flight)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_PERSON_CHECK, R.drawable.person_check)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_VEHICLE, R.drawable.directions_car)
