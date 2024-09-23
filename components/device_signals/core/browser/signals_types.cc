// Copyright 2022 The Chromium Authors
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
    case SignalCollectionError::kParsingFailed:
      return errors::kParsingFailed;
    case SignalCollectionError::kUnexpectedValue:
      return errors::kUnexpectedValue;
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

GetSettingsOptions::GetSettingsOptions() = default;
GetSettingsOptions::GetSettingsOptions(const GetSettingsOptions&) = default;

GetSettingsOptions& GetSettingsOptions::operator=(const GetSettingsOptions&) =
    default;

GetSettingsOptions::~GetSettingsOptions() = default;

bool GetSettingsOptions::operator==(const GetSettingsOptions& other) const {
  return path == other.path && key == other.key &&
         get_value == other.get_value && hive == other.hive;
}

SettingsItem::SettingsItem() = default;

SettingsItem::SettingsItem(const SettingsItem& other) {
  path = other.path;
  key = other.key;
  hive = other.hive;
  presence = other.presence;
  if (other.setting_json_value) {
    setting_json_value = other.setting_json_value;
  }
}

SettingsItem& SettingsItem::operator=(const SettingsItem& other) {
  path = other.path;
  key = other.key;
  hive = other.hive;
  presence = other.presence;
  if (other.setting_json_value) {
    setting_json_value = other.setting_json_value;
  }
  return *this;
}

SettingsItem::~SettingsItem() = default;

bool SettingsItem::operator==(const SettingsItem& other) const {
  return path == other.path && presence == other.presence && key == other.key &&
         hive == other.hive && setting_json_value == other.setting_json_value;
}

SettingsResponse::SettingsResponse() = default;
SettingsResponse::SettingsResponse(const SettingsResponse&) = default;

SettingsResponse& SettingsResponse::operator=(const SettingsResponse&) =
    default;

SettingsResponse::~SettingsResponse() = default;

FileSystemInfoResponse::FileSystemInfoResponse() = default;
FileSystemInfoResponse::FileSystemInfoResponse(const FileSystemInfoResponse&) =
    default;

FileSystemInfoResponse& FileSystemInfoResponse::operator=(
    const FileSystemInfoResponse&) = default;

FileSystemInfoResponse::~FileSystemInfoResponse() = default;

AgentSignalsResponse::AgentSignalsResponse() = default;
AgentSignalsResponse::AgentSignalsResponse(const AgentSignalsResponse&) =
    default;

AgentSignalsResponse& AgentSignalsResponse::operator=(
    const AgentSignalsResponse&) = default;

AgentSignalsResponse::~AgentSignalsResponse() = default;

SignalsAggregationRequest::SignalsAggregationRequest() = default;

SignalsAggregationRequest::SignalsAggregationRequest(
    const SignalsAggregationRequest&) = default;

SignalsAggregationRequest::SignalsAggregationRequest(
    SignalsAggregationRequest&&) = default;

SignalsAggregationRequest& SignalsAggregationRequest::operator=(
    SignalsAggregationRequest&&) = default;

SignalsAggregationRequest::~SignalsAggregationRequest() = default;

bool SignalsAggregationRequest::operator==(
    const SignalsAggregationRequest& other) const {
  return signal_names == other.signal_names &&
         file_system_signal_parameters == other.file_system_signal_parameters &&
         settings_signal_parameters == other.settings_signal_parameters;
}

SignalsAggregationResponse::SignalsAggregationResponse() = default;

SignalsAggregationResponse::SignalsAggregationResponse(
    const SignalsAggregationResponse&) = default;

SignalsAggregationResponse::SignalsAggregationResponse(
    SignalsAggregationResponse&&) = default;

SignalsAggregationResponse& SignalsAggregationResponse::operator=(
    const SignalsAggregationResponse&) = default;

SignalsAggregationResponse& SignalsAggregationResponse::operator=(
    SignalsAggregationResponse&&) = default;

SignalsAggregationResponse::~SignalsAggregationResponse() = default;

}  // namespace device_signals
