// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ASSETS_LOAD_STATUS_H_
#define CHROME_BROWSER_VR_ASSETS_LOAD_STATUS_H_

namespace vr {

// Status of loading assets.
enum class AssetsLoadStatus : int {
  kSuccess = 0,       // Assets loaded successfully.
  kParseFailure = 1,  // Failed to load assets.
  kInvalidContent =
      2,  // Content of assets files is invalid, e.g. it couldn't be decoded.
  kNotFound = 3,  // Could not find asset files.
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ASSETS_LOAD_STATUS_H_
