// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPEN_FROM_CLIPBOARD_FAKE_CLIPBOARD_RECENT_CONTENT_H_
#define COMPONENTS_OPEN_FROM_CLIPBOARD_FAKE_CLIPBOARD_RECENT_CONTENT_H_

#include "base/time/time.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

// FakeClipboardRecentContent implements ClipboardRecentContent interface by
// returning configurable values for use by tests.
class FakeClipboardRecentContent : public ClipboardRecentContent {
 public:
  FakeClipboardRecentContent();

  FakeClipboardRecentContent(const FakeClipboardRecentContent&) = delete;
  FakeClipboardRecentContent& operator=(const FakeClipboardRecentContent&) =
      delete;

  ~FakeClipboardRecentContent() override;

  // ClipboardRecentContent implementation.
  std::optional<GURL> GetRecentURLFromClipboard() override;
  std::optional<std::u16string> GetRecentTextFromClipboard() override;
  void GetRecentImageFromClipboard(GetRecentImageCallback callback) override;
  std::optional<std::set<ClipboardContentType>> GetCachedClipboardContentTypes()
      override;
  bool HasRecentImageFromClipboard() override;
  void HasRecentContentFromClipboard(std::set<ClipboardContentType> types,
                                     HasDataCallback callback) override;
  void GetRecentURLFromClipboard(GetRecentURLCallback callback) override;
  void GetRecentTextFromClipboard(GetRecentTextCallback callback) override;
  base::TimeDelta GetClipboardContentAge() const override;
  void SuppressClipboardContent() override;
  void ClearClipboardContent() override;

  // Sets the URL and clipboard content age. This clears the text and image.
  void SetClipboardURL(const GURL& url, base::TimeDelta content_age);
  // Sets the text and clipboard content age. This clears the URL and image.
  void SetClipboardText(const std::u16string& text,
                        base::TimeDelta content_age);
  // Sets the image and clipboard content age. This clears the URL and text.
  void SetClipboardImage(const gfx::Image& image, base::TimeDelta content_age);

 private:
  std::optional<GURL> clipboard_url_content_;
  std::optional<std::u16string> clipboard_text_content_;
  std::optional<gfx::Image> clipboard_image_content_;
  base::TimeDelta content_age_;
  bool suppress_content_;
};

#endif  // COMPONENTS_OPEN_FROM_CLIPBOARD_FAKE_CLIPBOARD_RECENT_CONTENT_H_
