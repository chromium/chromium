// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/file_system_access/file_system_access_ui_helpers.h"

#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/url_identity.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"
#include "url/gurl.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/layout/layout_provider.h"
#endif

namespace {

base::FilePath GetPathForDisplayAsPath(const content::PathInfo& path_info) {
  // Use display_name for android content-URIs.
#if BUILDFLAG(IS_ANDROID)
  if (path_info.path.IsContentUri()) {
    return base::FilePath(path_info.display_name);
  }
#endif

  // Display the drive letter if the path is the root of the filesystem.
  auto dir_name = path_info.path.DirName();
  if (!path_info.path.empty() &&
      (dir_name.empty() || path_info.path == dir_name)) {
    return path_info.path;
  }

  return path_info.path.BaseName();
}

// Expected URL types for `UrlIdentity::CreateFromUrl()`.
constexpr UrlIdentity::TypeSet kUrlIdentityAllowedTypes = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kFile,
    UrlIdentity::Type::kIsolatedWebApp, UrlIdentity::Type::kChromeExtension};

constexpr UrlIdentity::FormatOptions kUrlIdentityOptions{
    .default_options = {
        UrlIdentity::DefaultFormatOptions::kOmitCryptographicScheme}};

}  // namespace

namespace file_system_access_ui_helper {

std::u16string GetElidedPathForDisplayAsTitle(
    const content::PathInfo& path_info) {
  // TODO(crbug.com/40254943): Consider moving filename elision logic into a
  // core component, which would allow for dynamic elision based on the _actual_
  // available pixel width and font of the dialog.
  //
  // Ensure file names containing spaces won't overflow to the next line in the
  // title of a permission prompt, which is very hard to read. File names not
  // containing a space will bump to the next line if the file name + preceding
  // text in the title is too long, which is still easy to read because the file
  // name is contiguous.
  int scalar_quarters =
      base::Contains(GetPathForDisplayAsPath(path_info).value(),
                     FILE_PATH_LITERAL(" "))
          ? 2
          : 3;
  std::optional<int> preferred_width;
#if defined(TOOLKIT_VIEWS)
  // views::LayoutProvider::Get() may be null in tests.
  if (views::LayoutProvider::Get()) {
    preferred_width = views::LayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  }
#endif
  const int available_pixel_width =
      preferred_width.value_or(400) * scalar_quarters / 4;
  return gfx::ElideFilename(GetPathForDisplayAsPath(path_info), gfx::FontList(),
                            available_pixel_width);
}

std::u16string GetPathForDisplayAsParagraph(
    const content::PathInfo& path_info) {
  // Paragraph text will wrap to the next line rather than overflow, so
  // there's no need to elide the file name.
  return GetPathForDisplayAsPath(path_info).LossyDisplayName();
}

std::u16string GetUrlIdentityName(Profile* profile, const GURL& url) {
  return UrlIdentity::CreateFromUrl(profile, url, kUrlIdentityAllowedTypes,
                                    kUrlIdentityOptions)
      .name;
}

}  // namespace file_system_access_ui_helper
