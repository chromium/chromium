// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_CROWDSTRIKE_CLIENT_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_CROWDSTRIKE_CLIENT_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"

namespace base {
class FilePath;
}  // namespace base

namespace device_signals {

struct CrowdStrikeSignals;
enum class SignalCollectionError;

// Client that can be used to retrieve information about a CrowdStrike agent
// installed on the device.
class CrowdStrikeClient {
 public:
  virtual ~CrowdStrikeClient() = default;

  static std::unique_ptr<CrowdStrikeClient> Create();
  static std::unique_ptr<CrowdStrikeClient> CreateForTesting(
      const base::FilePath& zta_file_path);

  // Will retrieve the CrowdStrike agent ID from the data.zta file, if it
  // exists. Will return the value via `callback`, or std::nullopt if nothing
  // could be found.
  virtual void GetIdentifiers(
      base::OnceCallback<void(std::optional<CrowdStrikeSignals>,
                              std::optional<SignalCollectionError>)>
          callback) = 0;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_CROWDSTRIKE_CLIENT_H_
