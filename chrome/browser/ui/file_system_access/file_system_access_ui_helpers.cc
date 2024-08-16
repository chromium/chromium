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
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"
#include "url/gurl.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/layout/layout_provider.h"
#endif

namespace {

base::FilePath GetPathForDisplayAsPath(const base::FilePath& path) {
  // Display the drive letter if the path is the root of the filesystem.
  auto dir_name = path.DirName();
  if (!path.empty() && (dir_name.empty() || path == dir_name)) {
    return path;
  }

  return path.BaseName();
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

std::u16string GetElidedPathForDisplayAsTitle(const base::FilePath& path) {
  // TODO(crbug.com/40254943): Consider moving filename elision logic into a
  // core component, which would allow for dynamic elision based on the _actual_
  // available pixel width and font of the dialog.
  //
  // Ensure file names containing spaces won't overflow to the next line in the
  // title of a permission prompt, which is very hard to read. File names not
  // containing a space will bump to the next line if the file name + preceding
  // text in the title is too long, which is still easy to read because the file
  // name is contiguous.
  int scalar_quarters = base::Contains(GetPathForDisplayAsPath(path).value(),
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
  return gfx::ElideFilename(GetPathForDisplayAsPath(path), gfx::FontList(),
                            available_pixel_width);
}

std::u16string GetPathForDisplayAsParagraph(const base::FilePath& path) {
  // Paragraph text will wrap to the next line rather than overflow, so
  // there's no need to elide the file name.
  return GetPathForDisplayAsPath(path).LossyDisplayName();
}

std::u16string GetUrlIdentityName(Profile* profile, const GURL& url) {
  return UrlIdentity::CreateFromUrl(profile, url, kUrlIdentityAllowedTypes,
                                    kUrlIdentityOptions)
      .name;
}

}  // namespace file_system_access_ui_helper
