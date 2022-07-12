// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/signals_types.h"

#include "components/device_signals/core/common/signals_constants.h"

namespace device_signals {

const std::string ErrorToString(SignalCollectionError error) {
  switch (error) {
    case SignalCollectionError::kConsentRequired:
      return errors::kConsentRequired;
    case SignalCollectionError::kUnaffiliatedUser:
      return errors::kUnaffiliatedUser;
    case SignalCollectionError::kUnsupported:
      return errors::kUnsupported;
    case SignalCollectionError::kMissingSystemService:
      return errors::kMissingSystemService;
    case SignalCollectionError::kMissingBundle:
      return errors::kMissingBundle;
    case SignalCollectionError::kInvalidUser:
      return errors::kInvalidUser;
    case SignalCollectionError::kMissingParameters:
      return errors::kMissingParameters;
  }
}

BaseSignalResponse::~BaseSignalResponse() = default;

#if BUILDFLAG(IS_WIN)
AntiVirusSignalResponse::AntiVirusSignalResponse() = default;
AntiVirusSignalResponse::AntiVirusSignalResponse(
    const AntiVirusSignalResponse&) = default;

AntiVirusSignalResponse& AntiVirusSignalResponse::operator=(
    const AntiVirusSignalResponse&) = default;

AntiVirusSignalResponse::~AntiVirusSignalResponse() = default;

HotfixSignalResponse::HotfixSignalResponse() = default;
HotfixSignalResponse::HotfixSignalResponse(const HotfixSignalResponse&) =
    default;

HotfixSignalResponse& HotfixSignalResponse::operator=(
    const HotfixSignalResponse&) = default;

HotfixSignalResponse::~HotfixSignalResponse() = default;
#endif  // BUILDFLAG(IS_WIN)

FileSystemInfoResponse::FileSystemInfoResponse() = default;
FileSystemInfoResponse::FileSystemInfoResponse(const FileSystemInfoResponse&) =
    default;

FileSystemInfoResponse& FileSystemInfoResponse::operator=(
    const FileSystemInfoResponse&) = default;

FileSystemInfoResponse::~FileSystemInfoResponse() = default;

SignalsAggregationRequest::SignalsAggregationRequest() = default;
SignalsAggregationRequest::SignalsAggregationRequest(
    const SignalsAggregationRequest&) = default;

SignalsAggregationRequest& SignalsAggregationRequest::operator=(
    const SignalsAggregationRequest&) = default;

SignalsAggregationRequest::~SignalsAggregationRequest() = default;

bool SignalsAggregationRequest::operator==(
    const SignalsAggregationRequest& other) const {
  return user_context == other.user_context &&
         signal_names == other.signal_names;
}

SignalsAggregationResponse::SignalsAggregationResponse() = default;
SignalsAggregationResponse::SignalsAggregationResponse(
    const SignalsAggregationResponse&) = default;

SignalsAggregationResponse& SignalsAggregationResponse::operator=(
    const SignalsAggregationResponse&) = default;

SignalsAggregationResponse::~SignalsAggregationResponse() = default;

}  // namespace device_signals
