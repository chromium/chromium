// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filename_generation/filename_generation.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/third_party/icu/icu_utf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "url/gurl.h"

namespace filename_generation {

namespace {

// The lower bound for file name truncation. If the truncation results in a name
// shorter than this limit, we give up automatic truncation and prompt the user.
const size_t kTruncatedNameLengthLowerbound = 5;

const base::FilePath::CharType kDefaultHtmlExtension[] =
    FILE_PATH_LITERAL("html");

// Check whether we can save page as complete-HTML for the contents which
// have specified a MIME type. Now only contents which have the MIME type
// "text/html" can be saved as complete-HTML.
bool CanSaveAsComplete(const std::string& contents_mime_type) {
  return contents_mime_type == "text/html" ||
         contents_mime_type == "application/xhtml+xml";
}

}  // namespace

const base::FilePath::CharType* ExtensionForMimeType(
    const std::string& contents_mime_type) {
  static const struct {
    const char* mime_type;
    const base::FilePath::CharType* suggested_extension;
  } kExtensions[] = {
      {"text/html", kDefaultHtmlExtension},
      {"text/xml", FILE_PATH_LITERAL("xml")},
      {"application/xhtml+xml", FILE_PATH_LITERAL("xhtml")},
      {"text/plain", FILE_PATH_LITERAL("txt")},
      {"text/css", FILE_PATH_LITERAL("css")},
      {"multipart/related", FILE_PATH_LITERAL("mhtml")},
  };
  for (const auto& extension : kExtensions) {
    if (contents_mime_type == extension.mime_type)
      return extension.suggested_extension;
  }
  return FILE_PATH_LITERAL("");
}

base::FilePath EnsureHtmlExtension(const base::FilePath& name) {
  base::FilePath::StringType ext = name.Extension();
  if (!ext.empty())
    ext.erase(ext.begin());  // Erase preceding '.'.
  std::string mime_type;
  if (!net::GetMimeTypeFromExtension(ext, &mime_type) ||
      !CanSaveAsComplete(mime_type)) {
    return base::FilePath(name.value() + FILE_PATH_LITERAL(".") +
                          kDefaultHtmlExtension);
  }
  return name;
}

base::FilePath EnsureMimeExtension(const base::FilePath& name,
                                   const std::string& contents_mime_type) {
  // Start extension at 1 to skip over period if non-empty.
  base::FilePath::StringType ext = name.Extension();
  if (!ext.empty())
    ext = ext.substr(1);
  base::FilePath::StringType suggested_extension =
      ExtensionForMimeType(contents_mime_type);
  std::string mime_type;
  if (!suggested_extension.empty() &&
      !net::GetMimeTypeFromExtension(ext, &mime_type)) {
    // Extension is absent or needs to be updated.
    return base::FilePath(name.value() + FILE_PATH_LITERAL(".") +
                          suggested_extension);
  }

  // Special treatment for MHTML: we would always want to add ".mhtml" as the
  // extension even if there's another recognized mime_type based on |ext|.
  // For example: the name is "page.html", we would like to have
  // "page.html.mhtml" instead of "page.html".
  if (contents_mime_type == "multipart/related" &&
      mime_type != "multipart/related") {
    return base::FilePath(name.value() + FILE_PATH_LITERAL(".mhtml"));
  }

  return name;
}

base::FilePath GenerateFilename(const std::u16string& title,
                                const GURL& url,
                                bool can_save_as_complete,
                                std::string contents_mime_type) {
  base::FilePath name_with_proper_ext = base::FilePath::FromUTF16Unsafe(title);

  // If the page's title matches its URL, use the URL. Try to use the last path
  // component or if there is none, the domain as the file name.
  // Normally we want to base the filename on the page title, or if it doesn't
  // exist, on the URL. It's not easy to tell if the page has no title, because
  // if the page has no title, WebContents::GetTitle() will return the page's
  // URL (adjusted for display purposes). Therefore, we convert the "title"
  // back to a URL, and if it matches the original page URL, we know the page
  // had no title (or had a title equal to its URL, which is fine to treat
  // similarly).
  if (title == url_formatter::FormatUrl(
                   url,
                   url_formatter::kFormatUrlOmitDefaults |
                       url_formatter::kFormatUrlOmitTrivialSubdomains |
                       url_formatter::kFormatUrlOmitHTTPS,
                   base::UnescapeRule::SPACES, nullptr, nullptr, nullptr)) {
    std::string url_path;
    if (!url.SchemeIs(url::kDataScheme)) {
      name_with_proper_ext = net::GenerateFileName(
          url, std::string(), std::string(), std::string(), contents_mime_type,
          std::string());

      // If host is used as file name, try to decode punycode.
      if (name_with_proper_ext.AsUTF8Unsafe() == url.host()) {
        name_with_proper_ext = base::FilePath::FromUTF16Unsafe(
            url_formatter::IDNToUnicode(url.host()));
      }
    } else {
      name_with_proper_ext = base::FilePath::FromUTF8Unsafe("dataurl");
    }
  }

  // Ask user for getting final saving name.
  name_with_proper_ext =
      EnsureMimeExtension(name_with_proper_ext, contents_mime_type);
  // Adjust extension for complete types.
  if (can_save_as_complete)
    name_with_proper_ext = EnsureHtmlExtension(name_with_proper_ext);

  base::FilePath::StringType file_name = name_with_proper_ext.value();
  base::i18n::ReplaceIllegalCharactersInPath(&file_name, '_');
  return base::FilePath(file_name);
}

bool TruncateFilename(base::FilePath* path, size_t limit) {
  base::FilePath basename(path->BaseName());
  // It is already short enough.
  if (basename.value().size() <= limit)
    return true;

  base::FilePath dir(path->DirName());
  base::FilePath::StringType ext(basename.Extension());
  base::FilePath::StringType name(basename.RemoveExtension().value());

  // Impossible to satisfy the limit.
  if (limit < kTruncatedNameLengthLowerbound + ext.size())
    return false;
  limit -= ext.size();

  // Encoding specific truncation logic.
  base::FilePath::StringType truncated;
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_APPLE)
  // UTF-8.
  base::TruncateUTF8ToByteSize(name, limit, &truncated);
#elif BUILDFLAG(IS_WIN)
  // UTF-16.
  DCHECK(name.size() > limit);
  truncated = name.substr(0, CBU16_IS_TRAIL(name[limit]) ? limit - 1 : limit);
#else
// We cannot generally assume that the file name encoding is in UTF-8 (see
// the comment for FilePath::AsUTF8Unsafe), hence no safe way to truncate.
#endif

  if (truncated.size() < kTruncatedNameLengthLowerbound)
    return false;
  *path = dir.Append(truncated + ext);
  return true;
}

}  // namespace filename_generation
