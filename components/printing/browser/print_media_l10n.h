// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_MEDIA_L10N_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_MEDIA_L10N_H_

#include <string>

namespace printing {

// Maps a paper vendor ID to a localized name; returns the localized
// name if any is found, else returns an empty string.
std::string LocalizePaperDisplayName(const std::string& vendor_id);

}  // namespace printing

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_MEDIA_L10N_H_
