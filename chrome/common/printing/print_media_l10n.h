// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRINTING_PRINT_MEDIA_L10N_H_
#define CHROME_COMMON_PRINTING_PRINT_MEDIA_L10N_H_

#include <string>
#include <vector>

#include "printing/backend/print_backend.h"

namespace printing {

enum class MediaSizeGroup {
  kSizeIn,
  kSizeMm,
  kSizeNamed,
};

struct MediaSizeInfo {
  std::u16string name;
  MediaSizeGroup sort_group;
  bool registered_size;
};

struct PaperWithSizeInfo {
  PaperWithSizeInfo(MediaSizeInfo msi, PrinterSemanticCapsAndDefaults::Paper p);

  MediaSizeInfo size_info;
  PrinterSemanticCapsAndDefaults::Paper paper;
};

// Maps a paper vendor ID to a localized name and sort group.  The returned name
// will be automatically generated if the vendor ID does not have a known
// mapping.  If the vendor ID is not a valid PWG self-describing media name,
// the returned name will be empty.  The returned names are u16strings to
// facilitate subsequent sorting; they need to be converted to UTF-8 before
// updating a `Paper` object.
MediaSizeInfo LocalizePaperDisplayName(const std::string& vendor_id);

// Sorts a list of paper sizes in place by using the paired sort groups.
void SortPaperDisplayNames(std::vector<PaperWithSizeInfo>& papers);

}  // namespace printing

#endif  // CHROME_COMMON_PRINTING_PRINT_MEDIA_L10N_H_
