// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_REPORTING_UTIL_ENCRYPTED_REPORTING_JSON_KEYS_H_
#define COMPONENTS_REPORTING_UTIL_ENCRYPTED_REPORTING_JSON_KEYS_H_

// This file contains JSON fields for the request and response to upload
// encrypted records to the reporting server. Request fields are listed after
// the request payload example. Response fields are listed after the response
// payload example. Fields present in both the request and response are placed
// with the request fields.

namespace reporting::json_keys {

// {{Note}} ERP Request Payload Overview
// {
//   "encryptedRecord": [
//     {
//       "encryptedWrappedRecord": "EncryptedMessage",
//       "encryptionInfo" : {
//         "encryptionKey": "LocalPublicValue",
//         "publicKeyId": 1
//       },
//       "sequenceInformation": {
//         "sequencingId": 1,
//         "generationId": 123456789,
//         "priority": 1
//         // The string value of the `generationGuid` may be empty for managed
//         // ChromeOS devices or any non-ChromeOS devices, but will always have
//         // a value for unmanaged ChromeOS devices. Its value, if present,
//         // must be a string of base::Uuid. See base/uuid.h for format
//         // information.
//         "generationGuid": "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
//       },
//       "compressionInformation": {
//         "compressionAlgorithm": 1
//       }
//     },
//     {
//       "encryptedWrappedRecord": "EncryptedMessage",
//       "encryptionInfo" : {
//         "encryptionKey": "LocalPublicValue",
//         "publicKeyId": 2
//       },
//       "sequenceInformation": {
//         "sequencingId": 2,
//         "generationId": 123456789,
//         "priority": 1,
//         "generationGuid": "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
//       },
//       "compressionInformation": {
//         "compressionAlgorithm": 1
//       }
//     }
//   ],
//   // optional field, corresponding to |need_encryption_keys| in
//   // components/reporting/proto/interface.proto
//   "attachEncryptionSettings": true,
//
//   "requestId": "SomeString",
//
//   // optional field, corresponding to the configuration file version
//   // that the client is holding at the moment.
//   "configurationFileVersion": 1234,
//
//   // optional field, only used by the client tast tests to signal to the
//   // server that this is an automated test from the lab. In production, this
//   // should always be absent. Even if it is erroneously present in production
//   // code, server ignores it. Marked as string to make it reusable in the
//   // future. Value should be "tast" in the tast tests.
//   "source": "SomeString",
//
//   // optional field, not present if the device is unmanaged.
//   "device": {
//     "clientId": "abcdef1234",
//     "dmToken": "abcdef1234",
//     "name": "George",
//     "osPlatform": "Windows",
//     "osVersion": "10.0.0.0"
//   },
//
//   "browser": {
//     "browserId": "abcdef1234",
//     "chromeVersion": "10.0.0.0",
//     "machineUser": "abcdef1234",
//     "userAgent": "abcdef1234"
//   }
// }
//
// The value of an "encryptedRecord" must be a list, in which each
// element is a dictionary that represents a record. The details
// of each record is documented in record.proto.

// High level request fields
inline constexpr char kEncryptedRecordList[] = "encryptedRecord";
inline constexpr char kAttachEncryptionSettings[] = "attachEncryptionSettings";
inline constexpr char kRequestId[] = "requestId";
inline constexpr char kConfigurationFileVersion[] = "configurationFileVersion";
inline constexpr char kSource[] = "source";
inline constexpr char kDevice[] = "device";
inline constexpr char kBrowser[] = "browser";

// fields inside "encryptedRecord"
inline constexpr char kEncryptedWrappedRecord[] = "encryptedWrappedRecord";
inline constexpr char kEncryptionInfo[] = "encryptionInfo";
inline constexpr char kSequenceInformation[] = "sequenceInformation";
inline constexpr char kCompressionInformation[] = "compressionInformation";

// fields inside "encryptionInfo"
inline constexpr char kEncryptionKey[] = "encryptionKey";
inline constexpr char kPublicKeyId[] = "publicKeyId";

// fields inside "compressionInformation"
inline constexpr char kCompressionAlgorithm[] = "compressionAlgorithm";

// fields inside "sequenceInformation"
inline constexpr char kSequencingId[] = "sequencingId";
inline constexpr char kGenerationId[] = "generationId";
inline constexpr char kPriority[] = "priority";
inline constexpr char kGenerationGuid[] = "generationGuid";

// fields inside "device"
inline constexpr char kClientId[] = "clientId";
inline constexpr char kDmToken[] = "dmToken";
inline constexpr char kDeviceName[] = "name";
inline constexpr char kOsPlatform[] = "osPlatform";
inline constexpr char kOsVersion[] = "osVersion";

// fields inside "browser"
inline constexpr char kBrowserId[] = "browserId";
inline constexpr char kChromeVersion[] = "chromeVersion";
inline constexpr char kMachineUser[] = "machineUser";
inline constexpr char kUserAgent[] = "userAgent";

// {{{Note}}} ERP Response Payload Overview
//
//  {
//    "lastSucceedUploadedRecord": ... // SequenceInformation proto
//    "firstFailedUploadedRecord": {
//      "failedUploadedRecord": ... // SequenceInformation proto
//      "failureStatus": ... // Status proto
//    },
//    "encryptionSettings": ... // EncryptionSettings proto
//    "forceConfirm": true, // if present, flag that lastSucceedUploadedRecord
//                          // is to be accepted unconditionally by client
//    "configurationFile": ... // ConfigurationFile proto
//    // Internal control
//    "enableUploadSizeAdjustment": true,  // If present, upload size
//                                         // adjustment is enabled.
//  }

// Succeeded upload
inline constexpr char kLastSucceedUploadedRecord[] =
    "lastSucceedUploadedRecord";

// Failed upload
inline constexpr char kFirstFailedUploadedRecord[] =
    "firstFailedUploadedRecord";
inline constexpr char kFailedUploadedRecord[] = "failedUploadedRecord";
inline constexpr char kFailureStatus[] = "failureStatus";
inline constexpr char kErrorCode[] = "code";
inline constexpr char kErrorMessage[] = "message";

// Encryption settings
inline constexpr char kEncryptionSettings[] = "encryptionSettings";

// Force confirm
inline constexpr char kForceConfirm[] = "forceConfirm";

// Configuration file proto
inline constexpr char kConfigurationFile[] = "configurationFile";
inline constexpr char kConfigurationFileSignature[] = "configFileSignature";
inline constexpr char kConfigurationFileMinimumReleaseVersion[] =
    "minimumReleaseVersion";
inline constexpr char kConfigurationFileMaximumReleaseVersion[] =
    "maximumReleaseVersion";
inline constexpr char kConfigurationFileDestination[] = "destination";
inline constexpr char kConfigurationFileVersionResponse[] = "version";
inline constexpr char kBlockedEventConfigs[] = "blockedEventConfigs";

// Public key
inline constexpr char kPublicKey[] = "publicKey";
inline constexpr char kPublicKeySignature[] = "publicKeySignature";

// Enable upload size adjustment
inline constexpr char kEnableUploadSizeAdjustment[] =
    "enableUploadSizeAdjustment";

}  // namespace reporting::json_keys

#endif  // COMPONENTS_REPORTING_UTIL_ENCRYPTED_REPORTING_JSON_KEYS_H_
