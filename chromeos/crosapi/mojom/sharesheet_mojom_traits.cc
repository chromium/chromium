// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/mojom/sharesheet_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

crosapi::mojom::SharesheetLaunchSource
EnumTraits<crosapi::mojom::SharesheetLaunchSource,
           sharesheet::LaunchSource>::ToMojom(sharesheet::LaunchSource input) {
  switch (input) {
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case sharesheet::LaunchSource::kUnknown:
      return crosapi::mojom::SharesheetLaunchSource::kUnknown;
    case sharesheet::LaunchSource::kWebShare:
      return crosapi::mojom::SharesheetLaunchSource::kWebShare;
    case sharesheet::LaunchSource::kOmniboxShare:
      return crosapi::mojom::SharesheetLaunchSource::kOmniboxShare;
  }
}

bool EnumTraits<crosapi::mojom::SharesheetLaunchSource,
                sharesheet::LaunchSource>::
    FromMojom(crosapi::mojom::SharesheetLaunchSource input,
              sharesheet::LaunchSource* output) {
  switch (input) {
    case crosapi::mojom::SharesheetLaunchSource::kUnknown:
      *output = sharesheet::LaunchSource::kUnknown;
      return true;
    case crosapi::mojom::SharesheetLaunchSource::kWebShare:
      *output = sharesheet::LaunchSource::kWebShare;
      return true;
    case crosapi::mojom::SharesheetLaunchSource::kOmniboxShare:
      *output = sharesheet::LaunchSource::kOmniboxShare;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

crosapi::mojom::SharesheetResult EnumTraits<
    crosapi::mojom::SharesheetResult,
    sharesheet::SharesheetResult>::ToMojom(sharesheet::SharesheetResult input) {
  switch (input) {
    case sharesheet::SharesheetResult::kSuccess:
      return crosapi::mojom::SharesheetResult::kSuccess;
    case sharesheet::SharesheetResult::kCancel:
      return crosapi::mojom::SharesheetResult::kCancel;
    case sharesheet::SharesheetResult::kErrorAlreadyOpen:
      return crosapi::mojom::SharesheetResult::kErrorAlreadyOpen;
    case sharesheet::SharesheetResult::kErrorWindowClosed:
      return crosapi::mojom::SharesheetResult::kErrorWindowClosed;
  }
  NOTREACHED_IN_MIGRATION();
}

bool EnumTraits<crosapi::mojom::SharesheetResult,
                sharesheet::SharesheetResult>::
    FromMojom(crosapi::mojom::SharesheetResult input,
              sharesheet::SharesheetResult* output) {
  switch (input) {
    case crosapi::mojom::SharesheetResult::kSuccess:
      *output = sharesheet::SharesheetResult::kSuccess;
      return true;
    case crosapi::mojom::SharesheetResult::kCancel:
      *output = sharesheet::SharesheetResult::kCancel;
      return true;
    case crosapi::mojom::SharesheetResult::kErrorAlreadyOpen:
      *output = sharesheet::SharesheetResult::kErrorAlreadyOpen;
      return true;
    case crosapi::mojom::SharesheetResult::kErrorWindowClosed:
      *output = sharesheet::SharesheetResult::kErrorWindowClosed;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
