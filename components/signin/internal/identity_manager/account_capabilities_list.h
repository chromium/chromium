// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate a list of constants. The following line silences a
// presubmit and Tricium warning that would otherwise be triggered by this:
// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

// This is the list of account capabilities identifiers and their values. For
// the constant declarations, include the file
// "account_capabilities_constants.h".

// Here we define the values using a macro ACCOUNT_CAPABILITY, so it can be
// expanded differently in some places. The macro has the following signature:
// ACCOUNT_CAPABILITY(cpp_label, java_label, name).

// Please keep the list alphabetically sorted by the capability identifiers.

ACCOUNT_CAPABILITY(kCanHaveEmailAddressDisplayedCapabilityName,
                   CAN_HAVE_EMAIL_ADDRESS_DISPLAYED_CAPABILITY_NAME,
                   "accountcapabilities/haytqlldmfya")

ACCOUNT_CAPABILITY(kCanOfferExtendedChromeSyncPromosCapabilityName,
                   CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS_CAPABILITY_NAME,
                   "accountcapabilities/gi2tklldmfya")

ACCOUNT_CAPABILITY(kCanRunChromePrivacySandboxTrialsCapabilityName,
                   CAN_RUN_CHROME_PRIVACY_SANDBOX_TRIALS_CAPABILITY_NAME,
                   "accountcapabilities/gu2dqlldmfya")

ACCOUNT_CAPABILITY(kCanStopParentalSupervisionCapabilityName,
                   CAN_STOP_PARENTAL_SUPERVISION_CAPABILITY_NAME,
                   "accountcapabilities/guzdslldmfya")

ACCOUNT_CAPABILITY(kCanToggleAutoUpdatesName,
                   CAN_TOGGLE_AUTO_UPDATES_NAME,
                   "accountcapabilities/gu4dmlldmfya")

ACCOUNT_CAPABILITY(kIsAllowedForMachineLearningCapabilityName,
                   IS_ALLOWED_FOR_MACHINE_LEARNING_CAPABILITY_NAME,
                   "accountcapabilities/g42tslldmfya")

ACCOUNT_CAPABILITY(kIsSubjectToEnterprisePoliciesCapabilityName,
                   IS_SUBJECT_TO_ENTERPRISE_POLICIES_CAPABILITY_NAME,
                   "accountcapabilities/g44tilldmfya")

ACCOUNT_CAPABILITY(kIsSubjectToParentalControlsCapabilityName,
                   IS_SUBJECT_TO_PARENTAL_CONTROLS_CAPABILITY_NAME,
                   "accountcapabilities/guydolldmfya")
