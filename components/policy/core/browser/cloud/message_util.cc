// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/cloud/message_util.h"

#include "base/notreached.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

namespace {

int GetIDSForDMStatus(DeviceManagementStatus status) {
  switch (status) {
    case DM_STATUS_SUCCESS:
      return IDS_POLICY_DM_STATUS_SUCCESS;
    case DM_STATUS_REQUEST_INVALID:
      return IDS_POLICY_DM_STATUS_REQUEST_INVALID;
    case DM_STATUS_REQUEST_FAILED:
      return IDS_POLICY_DM_STATUS_REQUEST_FAILED;
    case DM_STATUS_TEMPORARY_UNAVAILABLE:
      return IDS_POLICY_DM_STATUS_TEMPORARY_UNAVAILABLE;
    case DM_STATUS_HTTP_STATUS_ERROR:
      return IDS_POLICY_DM_STATUS_HTTP_STATUS_ERROR;
    case DM_STATUS_RESPONSE_DECODING_ERROR:
      return IDS_POLICY_DM_STATUS_RESPONSE_DECODING_ERROR;
    case DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
      return IDS_POLICY_DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED;
    case DM_STATUS_SERVICE_DEVICE_NOT_FOUND:
      return IDS_POLICY_DM_STATUS_SERVICE_DEVICE_NOT_FOUND;
    case DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID:
      return IDS_POLICY_DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID;
    case DM_STATUS_SERVICE_ACTIVATION_PENDING:
      return IDS_POLICY_DM_STATUS_SERVICE_ACTIVATION_PENDING;
    case DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER:
      return IDS_POLICY_DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER;
    case DM_STATUS_SERVICE_DEVICE_ID_CONFLICT:
      return IDS_POLICY_DM_STATUS_SERVICE_DEVICE_ID_CONFLICT;
    case DM_STATUS_SERVICE_MISSING_LICENSES:
      return IDS_POLICY_DM_STATUS_SERVICE_MISSING_LICENSES;
    case DM_STATUS_SERVICE_DEPROVISIONED:
      return IDS_POLICY_DM_STATUS_SERVICE_DEPROVISIONED;
    case DM_STATUS_SERVICE_DOMAIN_MISMATCH:
      return IDS_POLICY_DM_STATUS_SERVICE_DOMAIN_MISMATCH;
    case DM_STATUS_SERVICE_POLICY_NOT_FOUND:
      return IDS_POLICY_DM_STATUS_SERVICE_POLICY_NOT_FOUND;
    case DM_STATUS_CANNOT_SIGN_REQUEST:
      return IDS_POLICY_DM_STATUS_CANNOT_SIGN_REQUEST;
    case DM_STATUS_REQUEST_TOO_LARGE:
      return IDS_POLICY_DM_STATUS_REQUEST_TOO_LARGE;
    case DM_STATUS_SERVICE_ARC_DISABLED:
      // This error is never shown on the UI.
      return IDS_POLICY_DM_STATUS_UNKNOWN_ERROR;
    case DM_STATUS_SERVICE_TOO_MANY_REQUESTS:
      return IDS_POLICY_DM_STATUS_SERVICE_TOO_MANY_REQUESTS;
    case DM_STATUS_SERVICE_DEVICE_NEEDS_RESET:
      return IDS_POLICY_DM_STATUS_SERVICE_DEVICE_NEEDS_RESET;
    case DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE:
      return IDS_POLICY_DM_STATUS_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE;
    case DM_STATUS_SERVICE_ENTERPRISE_ACCOUNT_IS_NOT_ELIGIBLE_TO_ENROLL:
      return IDS_POLICY_DM_STATUS_ENTERPRISE_ACCOUNT_IS_NOT_ELIGIBLE_TO_ENROLL;
    case DM_STATUS_SERVICE_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED:
      // This is shown only on registration failed.
      return IDS_POLICY_DM_STATUS_UNKNOWN_ERROR;
    case DM_STATUS_SERVICE_INVALID_PACKAGED_DEVICE_FOR_KIOSK:
      return IDS_POLICY_DM_STATUS_INVALID_PACKAGED_DEVICE_FOR_KIOSK;
    case DM_STATUS_SERVICE_ILLEGAL_ACCOUNT_FOR_PACKAGED_EDU_LICENSE:
      return IDS_POLICY_DM_STATUS_SERVICE_DOMAIN_MISMATCH;
  }
  NOTREACHED_IN_MIGRATION() << "Unhandled DM status " << status;
  return IDS_POLICY_DM_STATUS_UNKNOWN_ERROR;
}

int GetIDSForValidationStatus(CloudPolicyValidatorBase::Status status) {
  switch (status) {
    case CloudPolicyValidatorBase::VALIDATION_OK:
      return IDS_POLICY_VALIDATION_OK;
    case CloudPolicyValidatorBase::VALIDATION_BAD_INITIAL_SIGNATURE:
      return IDS_POLICY_VALIDATION_BAD_INITIAL_SIGNATURE;
    case CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE:
      return IDS_POLICY_VALIDATION_BAD_SIGNATURE;
    case CloudPolicyValidatorBase::VALIDATION_ERROR_CODE_PRESENT:
      return IDS_POLICY_VALIDATION_ERROR_CODE_PRESENT;
    case CloudPolicyValidatorBase::VALIDATION_PAYLOAD_PARSE_ERROR:
      return IDS_POLICY_VALIDATION_PAYLOAD_PARSE_ERROR;
    case CloudPolicyValidatorBase::VALIDATION_WRONG_POLICY_TYPE:
      return IDS_POLICY_VALIDATION_WRONG_POLICY_TYPE;
    case CloudPolicyValidatorBase::VALIDATION_WRONG_SETTINGS_ENTITY_ID:
      return IDS_POLICY_VALIDATION_WRONG_SETTINGS_ENTITY_ID;
    case CloudPolicyValidatorBase::VALIDATION_BAD_TIMESTAMP:
      return IDS_POLICY_VALIDATION_BAD_TIMESTAMP;
    case CloudPolicyValidatorBase::VALIDATION_BAD_DM_TOKEN:
      return IDS_POLICY_VALIDATION_BAD_DM_TOKEN;
    case CloudPolicyValidatorBase::VALIDATION_BAD_DEVICE_ID:
      return IDS_POLICY_VALIDATION_BAD_DEVICE_ID;
    case CloudPolicyValidatorBase::VALIDATION_BAD_USER:
      return IDS_POLICY_VALIDATION_BAD_USER;
    case CloudPolicyValidatorBase::VALIDATION_POLICY_PARSE_ERROR:
      return IDS_POLICY_VALIDATION_POLICY_PARSE_ERROR;
    case CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE:
      return IDS_POLICY_VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE;
    case CloudPolicyValidatorBase::VALIDATION_VALUE_WARNING:
      return IDS_POLICY_VALIDATION_VALUE_WARNING;
    case CloudPolicyValidatorBase::VALIDATION_VALUE_ERROR:
      return IDS_POLICY_VALIDATION_VALUE_ERROR;
    case CloudPolicyValidatorBase::VALIDATION_STATUS_SIZE:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION() << "Unhandled validation status " << status;
  return IDS_POLICY_VALIDATION_UNKNOWN_ERROR;
}

int GetIDSForStoreStatus(CloudPolicyStore::Status status) {
  switch (status) {
    case CloudPolicyStore::STATUS_OK:
      return IDS_POLICY_STORE_STATUS_OK;
    case CloudPolicyStore::STATUS_LOAD_ERROR:
      return IDS_POLICY_STORE_STATUS_LOAD_ERROR;
    case CloudPolicyStore::STATUS_STORE_ERROR:
      return IDS_POLICY_STORE_STATUS_STORE_ERROR;
    case CloudPolicyStore::STATUS_PARSE_ERROR:
      return IDS_POLICY_STORE_STATUS_PARSE_ERROR;
    case CloudPolicyStore::STATUS_SERIALIZE_ERROR:
      return IDS_POLICY_STORE_STATUS_SERIALIZE_ERROR;
    case CloudPolicyStore::STATUS_VALIDATION_ERROR:
      // This is handled separately below to include the validation error.
      break;
    case CloudPolicyStore::STATUS_BAD_STATE:
      return IDS_POLICY_STORE_STATUS_BAD_STATE;
  }
  NOTREACHED_IN_MIGRATION() << "Unhandled store status " << status;
  return IDS_POLICY_STORE_STATUS_UNKNOWN_ERROR;
}

}  // namespace

std::u16string FormatDeviceManagementStatus(DeviceManagementStatus status) {
  return l10n_util::GetStringUTF16(GetIDSForDMStatus(status));
}

std::u16string FormatValidationStatus(
    CloudPolicyValidatorBase::Status validation_status) {
  return l10n_util::GetStringUTF16(
      GetIDSForValidationStatus(validation_status));
}

std::u16string FormatStoreStatus(
    CloudPolicyStore::Status store_status,
    CloudPolicyValidatorBase::Status validation_status) {
  if (store_status == CloudPolicyStore::STATUS_VALIDATION_ERROR) {
    return l10n_util::GetStringFUTF16(
        IDS_POLICY_STORE_STATUS_VALIDATION_ERROR,
        l10n_util::GetStringUTF16(
            GetIDSForValidationStatus(validation_status)));
  }

  return l10n_util::GetStringUTF16(GetIDSForStoreStatus(store_status));
}

}  // namespace policy
