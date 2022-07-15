// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_TYPES_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_TYPES_H_

#include <unordered_set>
#include <vector>

#include "build/build_config.h"
#include "components/device_signals/core/browser/user_context.h"
#include "components/device_signals/core/common/common_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include "components/device_signals/core/common/win/win_types.h"
#endif  // BUILDFLAG(IS_WIN)

namespace device_signals {

// Enum of names representing signals bundles that can be aggregated via the
// SignalsAggregator.
// These values are persisted to logs and should not be renumbered. Please
// update the DeviceSignalsSignalName enum in enums.xml when adding a new
// value here.
enum class SignalName {
  kAntiVirus,
  kHotfixes,
  kFileSystemInfo,
  kSystemSettings,
  kMaxValue = kSystemSettings
};

// Superset of all signal collection errors that can occur, including top-level
// as well as per-bundle errors.
// These values are persisted to logs and should not be renumbered. Please
// update the DeviceSignalsSignalCollectionError enum in enums.xml when adding a
// new value here.
enum class SignalCollectionError {
  kConsentRequired,
  kUnaffiliatedUser,
  kUnsupported,
  kMissingSystemService,
  kMissingBundle,
  kInvalidUser,
  kMissingParameters,
  kMaxValue = kMissingParameters
};

const std::string ErrorToString(SignalCollectionError error);

// Base struct type that each specific signal bundle types should extend. The
// derived signal bundles/responses should group a set of signals that
// cohesively belong together (e.g. device-level signals, policy values
// signals).
struct BaseSignalResponse {
  virtual ~BaseSignalResponse();

  // If set, represents a collection error that occurred while getting the
  // signal.
  absl::optional<SignalCollectionError> collection_error = absl::nullopt;
};

#if BUILDFLAG(IS_WIN)
struct AntiVirusSignalResponse : BaseSignalResponse {
  AntiVirusSignalResponse();

  AntiVirusSignalResponse(const AntiVirusSignalResponse&);
  AntiVirusSignalResponse& operator=(const AntiVirusSignalResponse&);

  ~AntiVirusSignalResponse() override;

  std::vector<AvProduct> av_products{};
};

struct HotfixSignalResponse : BaseSignalResponse {
  HotfixSignalResponse();

  HotfixSignalResponse(const HotfixSignalResponse&);
  HotfixSignalResponse& operator=(const HotfixSignalResponse&);

  ~HotfixSignalResponse() override;

  std::vector<InstalledHotfix> hotfixes{};
};
#endif  // BUILDFLAG(IS_WIN)

struct FileSystemInfoResponse : BaseSignalResponse {
  FileSystemInfoResponse();

  FileSystemInfoResponse(const FileSystemInfoResponse&);
  FileSystemInfoResponse& operator=(const FileSystemInfoResponse&);

  ~FileSystemInfoResponse() override;

  std::vector<FileSystemItem> file_system_items{};
};

// Request struct containing properties that will be used by the
// SignalAggregator to validate signals access permissions while delegating
// the collection to the right Collectors. Signals that require parameters (e.g.
// FileSystemInfo) will look for them in this object.
struct SignalsAggregationRequest {
  SignalsAggregationRequest();

  SignalsAggregationRequest(const SignalsAggregationRequest&);
  SignalsAggregationRequest& operator=(const SignalsAggregationRequest&);

  ~SignalsAggregationRequest();

  // Information about the user for whom these signals are collected.
  UserContext user_context{};

  // Names of the signals that need to be collected.
  std::unordered_set<SignalName> signal_names{};

  // Parameters required when requesting the collection of signals living on
  // the device's file system.
  std::vector<GetFileSystemInfoOptions> file_system_signal_parameters{};

  bool operator==(const SignalsAggregationRequest& other) const;
};

// Response from a signal collection request sent through the SignalsAggregator.
// The signal bundles on this object will be set according to the set of signal
// names given in the corresponding `SignalsAggregationRequest`.
struct SignalsAggregationResponse {
  SignalsAggregationResponse();

  SignalsAggregationResponse(const SignalsAggregationResponse&);
  SignalsAggregationResponse& operator=(const SignalsAggregationResponse&);

  ~SignalsAggregationResponse();

  // If set, represents an error that occurred before any signal could be
  // collected.
  absl::optional<SignalCollectionError> top_level_error = absl::nullopt;

#if BUILDFLAG(IS_WIN)
  absl::optional<AntiVirusSignalResponse> av_signal_response = absl::nullopt;
  absl::optional<HotfixSignalResponse> hotfix_signal_response = absl::nullopt;
#endif  // BUILDFLAG(IS_WIN)
  absl::optional<FileSystemInfoResponse> file_system_info_response =
      absl::nullopt;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_SIGNALS_TYPES_H_
