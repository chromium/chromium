// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/common/file_icon_util.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/file_manager/file_manager_resource_util.h"
#include "ui/file_manager/grit/file_manager_resources.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_icon_image_loader.h"

namespace app_list {
namespace internal {

enum class Icon {
  AUDIO,
  ARCHIVE,
  CHART,
  EXCEL,
  FOLDER,
  FORM,
  GDOC,
  GDRAW,
  GENERIC,
  GSHEET,
  GSITE,
  GSLIDES,
  GTABLE,
  IMAGE,
  MAP,
  PDF,
  PPT,
  SCRIPT,
  SITES,
  TINI,
  VIDEO,
  WORD,
};

int GetIconResourceIdForLocalFilePath(const base::FilePath& filepath) {
  // Changes to these three maps should be reflected in
  // ui/file_manager/file_manager/common/js/file_type.js.

  static const base::NoDestructor<base::flat_map<std::string, Icon>>
      extension_to_icon({
          // Image
          {".JPEG", Icon::IMAGE},
          {".JPG", Icon::IMAGE},
          {".BMP", Icon::IMAGE},
          {".GIF", Icon::IMAGE},
          {".ICO", Icon::IMAGE},
          {".PNG", Icon::IMAGE},
          {".WEBP", Icon::IMAGE},
          {".TIFF", Icon::IMAGE},
          {".TIF", Icon::IMAGE},
          {".SVG", Icon::IMAGE},

          // Raw
          {".ARW", Icon::IMAGE},
          {".CR2", Icon::IMAGE},
          {".DNG", Icon::IMAGE},
          {".NEF", Icon::IMAGE},
          {".NRW", Icon::IMAGE},
          {".ORF", Icon::IMAGE},
          {".RAF", Icon::IMAGE},
          {".RW2", Icon::IMAGE},

          // Video
          {".3GP", Icon::VIDEO},
          {".3GPP", Icon::VIDEO},
          {".AVI", Icon::VIDEO},
          {".MOV", Icon::VIDEO},
          {".MKV", Icon::VIDEO},
          {".MP4", Icon::VIDEO},
          {".M4V", Icon::VIDEO},
          {".MPG", Icon::VIDEO},
          {".MPEG", Icon::VIDEO},
          {".MPG4", Icon::VIDEO},
          {".MPEG4", Icon::VIDEO},
          {".OGM", Icon::VIDEO},
          {".OGV", Icon::VIDEO},
          {".OGX", Icon::VIDEO},
          {".WEBM", Icon::VIDEO},

          // Audio
          {".AMR", Icon::AUDIO},
          {".FLAC", Icon::AUDIO},
          {".MP3", Icon::AUDIO},
          {".M4A", Icon::AUDIO},
          {".OGA", Icon::AUDIO},
          {".OGG", Icon::AUDIO},
          {".WAV", Icon::AUDIO},

          // Text
          {".TXT", Icon::GENERIC},

          // Archive
          {".ZIP", Icon::ARCHIVE},
          {".RAR", Icon::ARCHIVE},
          {".TAR", Icon::ARCHIVE},
          {".TAR.BZ2", Icon::ARCHIVE},
          {".TBZ", Icon::ARCHIVE},
          {".TBZ2", Icon::ARCHIVE},
          {".TAR.GZ", Icon::ARCHIVE},
          {".TGZ", Icon::ARCHIVE},

          // Hosted doc
          {".GDOC", Icon::GDOC},
          {".GSHEET", Icon::GSHEET},
          {".GSLIDES", Icon::GSLIDES},
          {".GDRAW", Icon::GDRAW},
          {".GTABLE", Icon::GTABLE},
          {".GLINK", Icon::GENERIC},
          {".GFORM", Icon::GENERIC},
          {".GMAPS", Icon::MAP},
          {".GSITE", Icon::GSITE},

          // Other
          {".PDF", Icon::PDF},
          {".HTM", Icon::GENERIC},
          {".HTML", Icon::GENERIC},
          {".MHT", Icon::GENERIC},
          {".MHTM", Icon::GENERIC},
          {".MHTML", Icon::GENERIC},
          {".SHTML", Icon::GENERIC},
          {".XHT", Icon::GENERIC},
          {".XHTM", Icon::GENERIC},
          {".XHTML", Icon::GENERIC},
          {".DOC", Icon::WORD},
          {".DOCX", Icon::WORD},
          {".PPT", Icon::PPT},
          {".PPTX", Icon::PPT},
          {".XLS", Icon::EXCEL},
          {".XLSX", Icon::EXCEL},
          {".TINI", Icon::TINI},
      });

  static const base::NoDestructor<base::flat_map<Icon, int>>
      icon_to_2x_resource_id({
          {Icon::AUDIO, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_AUDIO},
          {Icon::ARCHIVE, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_ARCHIVE},
          {Icon::CHART, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_CHART},
          {Icon::EXCEL, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_EXCEL},
          {Icon::FOLDER, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_FOLDER},
          {Icon::FORM, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_FORM},
          {Icon::GDOC, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GDOC},
          {Icon::GDRAW, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GDRAW},
          {Icon::GENERIC, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GENERIC},
          {Icon::GSHEET, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GSHEET},
          {Icon::GSITE, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GSITE},
          {Icon::GSLIDES, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GSLIDES},
          {Icon::GTABLE, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GTABLE},
          {Icon::IMAGE, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_IMAGE},
          {Icon::MAP, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_MAP},
          {Icon::PDF, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_PDF},
          {Icon::PPT, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_PPT},
          {Icon::SCRIPT, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_SCRIPT},
          {Icon::SITES, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_SITES},
          {Icon::TINI, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_TINI},
          {Icon::VIDEO, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_VIDEO},
          {Icon::WORD, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_WORD},
      });

  Icon icon;
  const auto& icon_it =
      extension_to_icon->find(base::ToUpperASCII(filepath.Extension()));
  if (icon_it != extension_to_icon->end())
    icon = icon_it->second;
  else
    icon = Icon::GENERIC;

  const auto& id_it = icon_to_2x_resource_id->find(icon);
  DCHECK(id_it != icon_to_2x_resource_id->end());
  return id_it->second;
}

}  // namespace internal

gfx::ImageSkia GetIconForLocalFilePath(const base::FilePath& filepath) {
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      internal::GetIconResourceIdForLocalFilePath(filepath));
}

}  // namespace app_list
