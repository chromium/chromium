// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_STRING_UTILS_H_
#define CHROME_BROWSER_UI_LENS_LENS_STRING_UTILS_H_

namespace lens {

// Returns the string ID for the Lens overlay entrypoint label, which may fall
// back to the provided default value.
int GetLensOverlayEntrypointLabelAltIds(int default_value);

// Returns the string ID for the Lens overlay image entrypoint label, which may
// fall back to the provided default value.
int GetLensOverlayImageEntrypointLabelAltIds(int default_value);

// Returns the string ID for the Lens overlay video entrypoint label, which
// may fall back to the provided default value.
int GetLensOverlayVideoEntrypointLabelAltIds(int default_value);

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_STRING_UTILS_H_
