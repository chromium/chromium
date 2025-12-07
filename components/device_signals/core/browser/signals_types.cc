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

bool BaseSignalResponse::operator==(const BaseSignalResponse&) const = default;

#if BUILDFLAG(IS_WIN)
AntiVirusSignalResponse::AntiVirusSignalResponse() = default;
AntiVirusSignalResponse::AntiVirusSignalResponse(
    const AntiVirusSignalResponse&) = default;

AntiVirusSignalResponse& AntiVirusSignalResponse::operator=(
    const AntiVirusSignalResponse&) = default;

bool AntiVirusSignalResponse::operator==(const AntiVirusSignalResponse&) const =
    default;

AntiVirusSignalResponse::~AntiVirusSignalResponse() = default;

HotfixSignalResponse::HotfixSignalResponse() = default;
HotfixSignalResponse::HotfixSignalResponse(const HotfixSignalResponse&) =
    default;

HotfixSignalResponse& HotfixSignalResponse::operator=(
    const HotfixSignalResponse&) = default;

bool HotfixSignalResponse::operator==(const HotfixSignalResponse&) const =
    default;

HotfixSignalResponse::~HotfixSignalResponse() = default;

#endif  // BUILDFLAG(IS_WIN)

GetSettingsOptions::GetSettingsOptions() = default;
GetSettingsOptions::GetSettingsOptions(const GetSettingsOptions&) = default;

GetSettingsOptions& GetSettingsOptions::operator=(const GetSettingsOptions&) =
    default;

bool GetSettingsOptions::operator==(const GetSettingsOptions&) const = default;

GetSettingsOptions::~GetSettingsOptions() = default;

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

SettingsItem& SettingsItem::operator=(const SettingsItem& other) = default;

bool SettingsItem::operator==(const SettingsItem&) const = default;

SettingsItem::~SettingsItem() = default;


SettingsResponse::SettingsResponse() = default;
SettingsResponse::SettingsResponse(const SettingsResponse&) = default;

SettingsResponse& SettingsResponse::operator=(const SettingsResponse&) =
    default;

bool SettingsResponse::operator==(const SettingsResponse&) const = default;

SettingsResponse::~SettingsResponse() = default;

OsSignalsResponse::OsSignalsResponse() = default;
OsSignalsResponse::OsSignalsResponse(const OsSignalsResponse&) = default;

OsSignalsResponse& OsSignalsResponse::operator=(const OsSignalsResponse&) =
    default;

bool OsSignalsResponse::operator==(const OsSignalsResponse&) const = default;

OsSignalsResponse::~OsSignalsResponse() = default;

ProfileSignalsResponse::ProfileSignalsResponse() = default;
ProfileSignalsResponse::ProfileSignalsResponse(const ProfileSignalsResponse&) =
    default;

ProfileSignalsResponse& ProfileSignalsResponse::operator=(
    const ProfileSignalsResponse&) = default;

bool ProfileSignalsResponse::operator==(const ProfileSignalsResponse&) const =
    default;

ProfileSignalsResponse::~ProfileSignalsResponse() = default;

FileSystemInfoResponse::FileSystemInfoResponse() = default;
FileSystemInfoResponse::FileSystemInfoResponse(const FileSystemInfoResponse&) =
    default;

FileSystemInfoResponse& FileSystemInfoResponse::operator=(
    const FileSystemInfoResponse&) = default;

bool FileSystemInfoResponse::operator==(const FileSystemInfoResponse&) const =
    default;

FileSystemInfoResponse::~FileSystemInfoResponse() = default;

AgentSignalsResponse::AgentSignalsResponse() = default;
AgentSignalsResponse::AgentSignalsResponse(const AgentSignalsResponse&) =
    default;

AgentSignalsResponse& AgentSignalsResponse::operator=(
    const AgentSignalsResponse&) = default;

bool AgentSignalsResponse::operator==(const AgentSignalsResponse&) const =
    default;

AgentSignalsResponse::~AgentSignalsResponse() = default;

SignalsAggregationRequest::SignalsAggregationRequest() = default;

SignalsAggregationRequest::SignalsAggregationRequest(
    const SignalsAggregationRequest&) = default;

SignalsAggregationRequest::SignalsAggregationRequest(
    SignalsAggregationRequest&&) = default;

SignalsAggregationRequest& SignalsAggregationRequest::operator=(
    SignalsAggregationRequest&&) = default;

bool SignalsAggregationRequest::operator==(
    const SignalsAggregationRequest&) const = default;

SignalsAggregationRequest::~SignalsAggregationRequest() = default;

SignalsAggregationResponse::SignalsAggregationResponse() = default;

SignalsAggregationResponse::SignalsAggregationResponse(
    const SignalsAggregationResponse&) = default;

SignalsAggregationResponse::SignalsAggregationResponse(
    SignalsAggregationResponse&&) = default;

SignalsAggregationResponse& SignalsAggregationResponse::operator=(
    const SignalsAggregationResponse&) = default;

SignalsAggregationResponse& SignalsAggregationResponse::operator=(
    SignalsAggregationResponse&&) = default;

bool SignalsAggregationResponse::operator==(
    const SignalsAggregationResponse&) const = default;

SignalsAggregationResponse::~SignalsAggregationResponse() = default;

}  // namespace device_signals
