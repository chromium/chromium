// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_REPORTING_UTIL_RECORD_UPLOAD_REQUEST_JSON_KEYS_H_
#define COMPONENTS_REPORTING_UTIL_RECORD_UPLOAD_REQUEST_JSON_KEYS_H_

// This file contains the JSON keys for a request to upload an encrypted record
// to the reporting server. A JSON version of the payload looks like this:
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
//         // The string value of the `generation_guid` may be empty for managed
//         // ChromeOS devices or any non-ChromeOS devices, but will always have
//         // a value for unmanaged ChromeOS devices. Its value, if present,
//         // must be a string of base::Uuid. See base/uuid.h for format
//         // information.
//         "generation_guid": "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
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
//         "generation_guid": "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
//       },
//       "compressionInformation": {
//         "compressionAlgorithm": 1
//       }
//     }
//   ],
//   // optional field, corresponding to |need_encryption_keys| in
//   // components/reporting/proto/interface.proto
//   "attachEncryptionSettings": true,
//   "requestId": "SomeString",
//   // optional field, corresponding to the configuration file version
//   // that the client is holding at the moment.
//   "configurationFileVersion": 1234
//   // optional field, only used by the client tast tests to signal to the
//   // server that this is an automated test from the lab. In production, this
//   // should always be absent. Even if it is erroneously present in production
//   // code, server ignores it. Marked as string to make it reusable in the
//   // future. Value should be "tast" in the tast tests.
//    "source": "SomeString"
// }
//
// This payload is added to the common payload of all reporting jobs, which
// includes other sub-fields such as "device" and "browser" (See note "ERP
// Payload Overview"):
//
//   EncryptedReportingRequestBuilder builder;
//   builder.AddRecord(record1);
//   builder.AddRecord(record2);
//   ...
//   builder.AddRecord(recordN);
//   auto payload_result = builder.Build();
//   CHECK(payload_result.has_value());
//   job_payload_.Merge(payload_result.value());
//
// The value of an "encryptedRecord" must be a list, in which each element is a
// dictionary that represents a record. The details of each record is documented
// in record.proto.

namespace reporting {
namespace json_keys {

// UploadEncryptedReportingRequestBuilder list key
inline constexpr char kEncryptedRecordListKey[] = "encryptedRecord";
inline constexpr char kAttachEncryptionSettingsKey[] =
    "attachEncryptionSettings";
inline constexpr char kConfigurationFileVersionKey[] =
    "configurationFileVersion";
inline constexpr char kSourceKey[] = "source";

// EncryptedRecordDictionaryBuilder strings
inline constexpr char kEncryptedWrappedRecordKey[] = "encryptedWrappedRecord";
inline constexpr char kSequenceInformationKey[] = "sequenceInformation";
inline constexpr char kEncryptionInfoKey[] = "encryptionInfo";
inline constexpr char kCompressionInformationKey[] = "compressionInformation";

// EncryptionInfoDictionaryBuilder strings
inline constexpr char kEncryptionKey[] = "encryptionKey";
inline constexpr char kPublicKeyId[] = "publicKeyId";

// CompressionInformationDictionaryBuilder strings
inline constexpr char kCompressionAlgorithmKey[] = "compressionAlgorithm";

// SequenceInformationDictionaryBuilder strings
inline constexpr char kSequencingIdKey[] = "sequencingId";
inline constexpr char kGenerationIdKey[] = "generationId";
inline constexpr char kPriorityKey[] = "priority";
inline constexpr char kGenerationGuidKey[] = "generationGuid";

// RequestId key used to build UploadEncryptedReportingRequest
inline constexpr char kRequestIdKey[] = "requestId";

// Key used for device info
inline constexpr char kDeviceKey[] = "device";

// Key used for browser info
inline constexpr char kBrowserKey[] = "browser";

}  // namespace json_keys
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_RECORD_UPLOAD_REQUEST_JSON_KEYS_H_
