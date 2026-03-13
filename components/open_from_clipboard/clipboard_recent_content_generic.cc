// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/open_from_clipboard/clipboard_recent_content_generic.h"

#include <string>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/base/clipboard/clipboard_android.h"
#endif

namespace {
// Schemes appropriate for suggestion by ClipboardRecentContent.
constexpr const char* kAuthorizedSchemes[] = {
    url::kAboutScheme, url::kDataScheme, url::kHttpScheme, url::kHttpsScheme,
    // TODO(mpearson): add support for chrome:// URLs.  Right now the scheme
    // for that lives in content and is accessible via
    // GetEmbedderRepresentationOfAboutScheme() or content::kChromeUIScheme
    // TODO(mpearson): when adding desktop support, add kFileScheme, kFtpScheme.
};

void OnGetRecentImageFromClipboard(
    ClipboardRecentContent::GetRecentImageCallback callback,
    const std::vector<uint8_t>& png_data) {
  if (png_data.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(gfx::Image::CreateFrom1xPNGBytes(png_data));
}

}  // namespace

ClipboardRecentContentGeneric::ClipboardRecentContentGeneric() = default;
ClipboardRecentContentGeneric::~ClipboardRecentContentGeneric() = default;

void ClipboardRecentContentGeneric::GetRecentURLFromClipboard(
    GetRecentURLCallback callback) {
  if (GetClipboardContentAge() > MaximumAgeOfClipboard()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
      ui::EndpointType::kDefault, {.notify_if_restricted = false});

#if BUILDFLAG(IS_ANDROID)
  ui::Clipboard::GetForCurrentThread()->ReadBookmark(
      std::move(data_dst),
      base::BindOnce(&ClipboardRecentContentGeneric::OnReadURL,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
#else
  ui::Clipboard::GetForCurrentThread()->ReadAsciiText(
      ui::ClipboardBuffer::kCopyPaste, std::move(data_dst),
      base::BindOnce(&ClipboardRecentContentGeneric::OnReadURLAsAsciiText,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
#endif  // BUILDFLAG(IS_ANDROID)
}

void ClipboardRecentContentGeneric::OnReadURLAsAsciiText(
    GetRecentURLCallback callback,
    std::string gurl_string) {
  base::TrimWhitespaceASCII(gurl_string, base::TrimPositions::TRIM_ALL,
                            &gurl_string);

  // If there is mid-string whitespace, don't attempt to interpret the string
  // as a URL.  (Otherwise gurl will happily try to convert
  // "http://example.com extra words" into "http://example.com%20extra%20words",
  // which is not likely to be a useful or intended destination.)
  GURL url;
  if (!gurl_string.empty() &&
      gurl_string.find_first_of(base::kWhitespaceASCII) == std::string::npos) {
    url = GURL(gurl_string);
  }

  OnReadURL(std::move(callback), u"", std::move(url));
}

void ClipboardRecentContentGeneric::OnReadText(GetRecentURLCallback callback,
                                               std::u16string gurl_string16) {
  base::TrimWhitespace(gurl_string16, base::TrimPositions::TRIM_ALL,
                       &gurl_string16);
  if (!gurl_string16.empty() &&
      gurl_string16.find_first_of(base::kWhitespaceUTF16) ==
          std::string::npos) {
    GURL url(gurl_string16);
    if (url.is_valid() && IsAppropriateSuggestion(url)) {
      std::move(callback).Run(std::move(url));
      return;
    }
  }
  std::move(callback).Run(std::nullopt);
}

void ClipboardRecentContentGeneric::GetRecentTextFromClipboard(
    GetRecentTextCallback callback) {
  if (GetClipboardContentAge() > MaximumAgeOfClipboard()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
      ui::EndpointType::kDefault, {.notify_if_restricted = false});
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, std::move(data_dst),
      base::BindOnce(
          [](GetRecentTextCallback callback, std::u16string text) {
            base::TrimWhitespace(text, base::TrimPositions::TRIM_ALL, &text);
            if (text.empty()) {
              std::move(callback).Run(std::nullopt);
            } else {
              std::move(callback).Run(std::move(text));
            }
          },
          std::move(callback)));
}

void ClipboardRecentContentGeneric::GetRecentImageFromClipboard(
    GetRecentImageCallback callback) {
  if (GetClipboardContentAge() > MaximumAgeOfClipboard()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
      ui::EndpointType::kDefault, {.notify_if_restricted = false});
  ui::Clipboard::GetForCurrentThread()->ReadPng(
      ui::ClipboardBuffer::kCopyPaste, data_dst,
      base::BindOnce(&OnGetRecentImageFromClipboard, std::move(callback)));
}

void ClipboardRecentContentGeneric::HasRecentImageFromClipboard(
    base::OnceCallback<void(bool)> callback) {
  if (GetClipboardContentAge() > MaximumAgeOfClipboard()) {
    std::move(callback).Run(false);
    return;
  }

  ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
      ui::EndpointType::kDefault, {.notify_if_restricted = false});
  ui::Clipboard::GetForCurrentThread()->GetAllAvailableFormats(
      ui::ClipboardBuffer::kCopyPaste, data_dst,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             base::flat_set<ui::ClipboardFormatType> formats) {
            std::move(callback).Run(
                formats.contains(ui::ClipboardFormatType::PngType()));
          },
          std::move(callback)));
}

void ClipboardRecentContentGeneric::HasRecentContentFromClipboard(
    std::set<ClipboardContentType> types,
    HasDataCallback callback) {
  if (GetClipboardContentAge() > MaximumAgeOfClipboard()) {
    std::move(callback).Run({});
    return;
  }

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
      ui::EndpointType::kDefault, {.notify_if_restricted = false});
  clipboard->GetAllAvailableFormats(
      ui::ClipboardBuffer::kCopyPaste, data_dst,
      base::BindOnce(
          [](std::set<ClipboardContentType> types, HasDataCallback callback,
             base::flat_set<ui::ClipboardFormatType> formats) {
            std::set<ClipboardContentType> matching_types;
            for (ClipboardContentType type : types) {
              switch (type) {
                case ClipboardContentType::URL:
                  if (formats.contains(ui::ClipboardFormatType::UrlType())) {
                    matching_types.insert(ClipboardContentType::URL);
                  }
                  break;
                case ClipboardContentType::Text:
                  if (formats.contains(
                          ui::ClipboardFormatType::PlainTextType())) {
                    matching_types.insert(ClipboardContentType::Text);
                  }
                  break;
                case ClipboardContentType::Image:
                  if (formats.contains(ui::ClipboardFormatType::PngType()) ||
                      formats.contains(ui::ClipboardFormatType::BitmapType())) {
                    matching_types.insert(ClipboardContentType::Image);
                  }
                  break;
              }
            }
            std::move(callback).Run(matching_types);
          },
          std::move(types), std::move(callback)));
}

base::TimeDelta ClipboardRecentContentGeneric::GetClipboardContentAge() const {
  const base::Time last_modified_time =
      ui::Clipboard::GetForCurrentThread()->GetLastModifiedTime();
  const base::Time now = base::Time::Now();
  // In case of system clock change, assume the last modified time is now.
  // (Don't return a negative age, i.e., a time in the future.)
  if (last_modified_time > now)
    return base::TimeDelta();
  return now - last_modified_time;
}

void ClipboardRecentContentGeneric::SuppressClipboardContent() {
  // User cleared the user data.  The pasteboard entry must be removed from the
  // omnibox list.  Do this by pretending the current clipboard is ancient,
  // not recent.
  ui::Clipboard::GetForCurrentThread()->ClearLastModifiedTime();
}

void ClipboardRecentContentGeneric::ClearClipboardContent() {
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
}

// static
bool ClipboardRecentContentGeneric::IsAppropriateSuggestion(const GURL& url) {
  // Check to make sure it's a scheme we're willing to suggest.
  for (const auto* authorized_scheme : kAuthorizedSchemes) {
    if (url.SchemeIs(authorized_scheme))
      return true;
  }

  // Check if the schemes is an application-defined scheme.
  std::vector<std::string> standard_schemes = url::GetStandardSchemes();
  for (const auto& standard_scheme : standard_schemes) {
    if (url.SchemeIs(standard_scheme))
      return true;
  }

  // Not a scheme we're allowed to return.
  return false;
}

void ClipboardRecentContentGeneric::OnReadURL(GetRecentURLCallback callback,
                                              std::u16string title,
                                              GURL url) {
  if (url.is_valid() && IsAppropriateSuggestion(url)) {
    std::move(callback).Run(std::move(url));
    return;
  }

  // Fall back to unicode / UTF16, as some URLs may use international domain
  // names, not punycode.
  ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
      ui::EndpointType::kDefault, {.notify_if_restricted = false});
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, std::move(data_dst),
      base::BindOnce(&ClipboardRecentContentGeneric::OnReadText,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}
