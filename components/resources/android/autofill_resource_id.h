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
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_AMEX, R.drawable.amex_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_DINERS, R.drawable.diners_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_DISCOVER, R.drawable.discover_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_ELO, R.drawable.elo_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_GENERIC, R.drawable.ic_credit_card_black)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_GENERIC_PRIMARY,
                 R.drawable.ic_credit_card_primary)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_JCB, R.drawable.jcb_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_MASTERCARD, R.drawable.mc_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_MIR, R.drawable.mir_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_TROY, R.drawable.troy_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_UNIONPAY, R.drawable.unionpay_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_VERVE, R.drawable.verve_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_CC_VISA, R.drawable.visa_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_GOOGLE_PAY, R.drawable.google_pay)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_AMEX, R.drawable.amex_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_CAPITALONE,
                 R.drawable.capitalone_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_DINERS,
                 R.drawable.diners_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_DISCOVER,
                 R.drawable.discover_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_ELO, R.drawable.elo_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_GENERIC,
                 R.drawable.ic_metadata_credit_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_JCB, R.drawable.jcb_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_MASTERCARD,
                 R.drawable.mc_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_MIR, R.drawable.mir_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_TROY, R.drawable.troy_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_UNIONPAY,
                 R.drawable.unionpay_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_VERVE, R.drawable.verve_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_METADATA_CC_VISA, R.drawable.visa_metadata_card)
LINK_RESOURCE_ID(IDR_AUTOFILL_IBAN, R.drawable.iban_icon)

// Use DECLARE_RESOURCE_ID here as these resources are used for android only.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_CC_SCAN_NEW,
                    R.drawable.ic_photo_camera_black)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_HTTP_WARNING,
                    R.drawable.ic_info_outline_grey_16dp)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_HTTPS_INVALID_WARNING,
                    R.drawable.ic_warning_red_16dp)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_OFFER_TAG_GREEN,
                    R.drawable.ic_offer_tag)

// Note that R.drawable.plus_addresses_logo is always present, but the icon in
// branded builds is different from the one in unbranded builds,
DECLARE_RESOURCE_ID(IDR_AUTOFILL_PLUS_ADDRESS,
                    R.drawable.ic_plus_addresses_logo_16dp)

// We display settings and edit icon for keyboard accessory using Android's
// |VectorDrawableCompat|. We do not display these icons for autofill popup.
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_SETTINGS, R.drawable.ic_settings_black)
DECLARE_RESOURCE_ID(IDR_ANDROID_AUTOFILL_CREATE, R.drawable.ic_edit_24dp)
