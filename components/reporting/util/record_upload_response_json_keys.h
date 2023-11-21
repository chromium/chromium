// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_REPORTING_UTIL_RECORD_UPLOAD_RESPONSE_JSON_KEYS_H_
#define COMPONENTS_REPORTING_UTIL_RECORD_UPLOAD_RESPONSE_JSON_KEYS_H_

// This file contains JSON fields for the server's response to a request to
// upload encrypted records. See
// components/reporting/util/record_upload_request_json_keys.h for details about
// the request.
namespace reporting {
namespace json_keys {
// Keys for response internal dictionaries
inline constexpr char kLastSucceedUploadedRecordKey[] =
    "lastSucceedUploadedRecord";
inline constexpr char kFirstFailedUploadedRecordKey[] =
    "firstFailedUploadedRecord";

// Keys for FirstFailedUploadRecord values.
inline constexpr char kFailedUploadedRecordKey[] = "failedUploadedRecord";
inline constexpr char kFailureStatusKey[] = "failureStatus";

// Keys for FirstFailedUploadRecord Status dictionary
inline constexpr char kCodeKey[] = "code";
inline constexpr char kMessageKey[] = "message";
}  // namespace json_keys
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_RECORD_UPLOAD_RESPONSE_JSON_KEYS_H_
