// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/background_task_origin.h"

#include <string_view>

namespace unexportable_keys {

std::string_view GetBackgroundTaskOriginSuffixForHistograms(
    BackgroundTaskOrigin origin) {
  // LINT.IfChange(BackgroundTaskOriginSuffixForHistograms)
  switch (origin) {
    case BackgroundTaskOrigin::kRefreshTokenBinding:
      return ".RefreshTokenBinding";
    case BackgroundTaskOrigin::kDeviceBoundSessionCredentials:
      return ".DeviceBoundSessions";
    case BackgroundTaskOrigin::kDeviceBoundSessionCredentialsPrototype:
      return ".BoundSessionCredentials";
    case BackgroundTaskOrigin::kOrphanedKeyGarbageCollection:
      return ".OrphanedKeyGarbageCollection";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/histograms.xml:UnexportableKeysBackgroundTaskOrigin)
}

}  // namespace unexportable_keys
