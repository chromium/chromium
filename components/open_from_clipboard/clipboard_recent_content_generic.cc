// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/open_from_clipboard/clipboard_recent_content_generic.h"

#include <string>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace {
// Schemes appropriate for suggestion by ClipboardRecentContent.
const char* kAuthorizedSchemes[] = {
    url::kAboutScheme, url::kDataScheme, url::kHttpScheme, url::kHttpsScheme,
    // TODO(mpearson): add support for chrome:// URLs.  Right now the scheme
    // for that lives in content and is accessible via
    // GetEmbedderRepresentationOfAboutScheme() or content::kChromeUIScheme
    // TODO(mpearson): when adding desktop support, add kFileScheme, kFtpScheme.
};

void OnGetRecentImageFromClipboard(
    ClipboardRecentContent::GetRecentImageCallback callback,
    const SkBitmap& sk_bitmap) {
  if (sk_bitmap.empty()) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  std::move(callback).Run(gfx::Image::CreateFrom1xBitmap(sk_bitmap));
}

}  // namespace

ClipboardRecentContentGeneric::ClipboardRecentContentGeneric() = default;
ClipboardRecentContentGeneric::~ClipboardRecentContentGeneric() = default;

base::Optional<GURL>
ClipboardRecentContentGeneric::GetRecentURLFromClipboard() {
  if (GetClipboardContentAge() > MaximumAgeOfClipboard())
    return base::nullopt;

  // Get and clean up the clipboard before processing.
  std::string gurl_string;
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
      ui::EndpointType::kDefault, /*notify_if_restricted=*/false);
  clipboard->ReadAsciiText(ui::ClipboardBuffer::kCopyPaste, &data_dst,
                           &gurl_string);
  base::TrimWhitespaceASCII(gurl_string, base::TrimPositions::TRIM_ALL,
                            &gurl_string);

  // Interpret the clipboard as a URL if possible.
  GURL url;
  // If there is mid-string whitespace, don't attempt to interpret the string
  // as a URL.  (Otherwise gurl will happily try to convert
  // "http://example.com extra words" into "http://example.com%20extra%20words",
  // which is not likely to be a useful or intended destination.)
  if (gurl_string.find_first_of(base::kWhitespaceASCII) != std::string::npos)
    return base::nullopt;
  if (!gurl_string.empty()) {
    url = GURL(gurl_string);
  } else {
    // Fall back to unicode / UTF16, as some URLs may use international domain
    // names, not punycode.
    base::string16 gurl_string16;
    clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, &data_dst,
                        &gurl_string16);
    base::TrimWhitespace(gurl_string16, base::TrimPositions::TRIM_ALL,
                         &gurl_string16);
    if (gurl_string16.find_first_of(base::kWhitespaceUTF16) !=
        std::string::npos)
      return base::nullopt;
    if (!gurl_string16.empty())
      url = GURL(gurl_string16);
  }
  if (!url.is_valid() || !IsAppropriateSuggestion(url)) {
    return base::nullopt;
  }
  return url;
}

base::Optional<base::string16>
ClipboardRecentContentGeneric::GetRecentTextFromClipboard() {
  if (GetClipboardContentAge() > MaximumAgeOfClipboard())
    return base::nullopt;

  base::string16 text_from_clipboard;
  ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
      ui::EndpointType::kDefault, /*notify_if_restricted=*/false);
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst, &text_from_clipboard);
  base::TrimWhitespace(text_from_clipboard, base::TrimPositions::TRIM_ALL,
                       &text_from_clipboard);
  if (text_from_clipboard.empty()) {
    return base::nullopt;
  }

  return text_from_clipboard;
}

void ClipboardRecentContentGeneric::GetRecentImageFromClipboard(
    GetRecentImageCallback callback) {
  if (GetClipboardContentAge() > MaximumAgeOfClipboard())
    return;

  ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
      ui::EndpointType::kDefault, /*notify_if_restricted=*/false);
  ui::Clipboard::GetForCurrentThread()->ReadImage(
      ui::ClipboardBuffer::kCopyPaste, &data_dst,
      base::BindOnce(&OnGetRecentImageFromClipboard, std::move(callback)));
}

bool ClipboardRecentContentGeneric::HasRecentImageFromClipboard() {
  if (GetClipboardContentAge() > MaximumAgeOfClipboard())
    return false;

  ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
      ui::EndpointType::kDefault, /*notify_if_restricted=*/false);
  return ui::Clipboard::GetForCurrentThread()->IsFormatAvailable(
      ui::ClipboardFormatType::GetBitmapType(), ui::ClipboardBuffer::kCopyPaste,
      &data_dst);
}

void ClipboardRecentContentGeneric::HasRecentContentFromClipboard(
    std::set<ClipboardContentType> types,
    HasDataCallback callback) {
  std::set<ClipboardContentType> matching_types;
  for (ClipboardContentType type : types) {
    switch (type) {
      case ClipboardContentType::URL:
        if (GetRecentURLFromClipboard()) {
          matching_types.insert(ClipboardContentType::URL);
        }
        break;
      case ClipboardContentType::Text:
        if (GetRecentTextFromClipboard()) {
          matching_types.insert(ClipboardContentType::Text);
        }
        break;
      case ClipboardContentType::Image:
        if (HasRecentImageFromClipboard()) {
          matching_types.insert(ClipboardContentType::Image);
        }
        break;
    }
  }
  std::move(callback).Run(matching_types);
}

void ClipboardRecentContentGeneric::GetRecentURLFromClipboard(
    GetRecentURLCallback callback) {
  std::move(callback).Run(GetRecentURLFromClipboard());
}

void ClipboardRecentContentGeneric::GetRecentTextFromClipboard(
    GetRecentTextCallback callback) {
  std::move(callback).Run(GetRecentTextFromClipboard());
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

  // Not a scheme we're allowed to return.
  return false;
}
