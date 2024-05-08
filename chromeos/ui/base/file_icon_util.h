// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_BASE_FILE_ICON_UTIL_H_
#define CHROMEOS_UI_BASE_FILE_ICON_UTIL_H_

#include <optional>

#include "base/files/file_path.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace chromeos {

enum class IconType {
  kAudio,
  kArchive,
  kChart,
  kDrive,
  kExcel,
  kFolder,
  kFolderShared,
  kGdoc,
  kGdraw,
  kGeneric,
  kGform,
  kGmap,
  kGsheet,
  kGsite,
  kGmaillayout,
  kGslide,
  kGtable,
  kLinux,
  kImage,
  kPdf,
  kPpt,
  kScript,
  kSites,
  kTini,
  kVideo,
  kWord,
};

namespace internal {

COMPONENT_EXPORT(CHROMEOS_UI_BASE)
IconType GetIconTypeFromString(const std::string& icon_type_string);

COMPONENT_EXPORT(CHROMEOS_UI_BASE)
IconType GetIconTypeForPath(const base::FilePath& filepath);

}  // namespace internal

// Returns the file type vector icon for the specified `file_path`.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
const gfx::VectorIcon& GetIconForPath(const base::FilePath& file_path);

// Returns the file type icon for the specified `file_path`. If
// `dark_background` is `true`, lighter foreground colors are used to ensure
// sufficient contrast.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
gfx::ImageSkia GetIconForPath(const base::FilePath& file_path,
                              bool dark_background,
                              std::optional<int> dip_size = {});

// Returns the file type chip icon for the specified `filepath`.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
gfx::ImageSkia GetChipIconForPath(const base::FilePath& filepath,
                                  bool dark_background);

// Returns the file type vector icon for the specified `icon_type`.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
const gfx::VectorIcon& GetIconFromType(const std::string& icon_type);

// Returns the file type icon for the specified `icon_type`.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
gfx::ImageSkia GetIconFromType(const std::string& icon_type,
                               bool dark_background);

// Returns the file type icon for the specified `icon_type`. If
// `dark_background` is `true`, lighter foreground colors are used to ensure
// sufficient contrast.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
gfx::ImageSkia GetIconFromType(IconType icon_type,
                               bool dark_background,
                               std::optional<int> dip_size = {});

// Returns the resolved color of the file type icon for the specified
// `filepath`. If `dark_background` is `true`, lighter foreground colors are
// used to ensure sufficient contrast.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
SkColor GetIconColorForPath(const base::FilePath& filepath,
                            bool dark_background);

}  // namespace chromeos

#endif  // CHROMEOS_UI_BASE_FILE_ICON_UTIL_H_
