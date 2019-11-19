// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPEN_FROM_CLIPBOARD_FAKE_CLIPBOARD_RECENT_CONTENT_H_
#define COMPONENTS_OPEN_FROM_CLIPBOARD_FAKE_CLIPBOARD_RECENT_CONTENT_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

// FakeClipboardRecentContent implements ClipboardRecentContent interface by
// returning configurable values for use by tests.
class FakeClipboardRecentContent : public ClipboardRecentContent {
 public:
  FakeClipboardRecentContent();
  ~FakeClipboardRecentContent() override;

  // ClipboardRecentContent implementation.
  base::Optional<GURL> GetRecentURLFromClipboard() override;
  base::Optional<base::string16> GetRecentTextFromClipboard() override;
  base::Optional<gfx::Image> GetRecentImageFromClipboard() override;
  base::TimeDelta GetClipboardContentAge() const override;
  void SuppressClipboardContent() override;
  void ClearClipboardContent() override;

  // Sets the URL and clipboard content age. This clears the text and image.
  void SetClipboardURL(const GURL& url, base::TimeDelta content_age);
  // Sets the text and clipboard content age. This clears the URL and image.
  void SetClipboardText(const base::string16& text,
                        base::TimeDelta content_age);
  // Sets the image and clipboard content age. This clears the URL and text.
  void SetClipboardImage(const gfx::Image& image, base::TimeDelta content_age);

 private:
  base::Optional<GURL> clipboard_url_content_;
  base::Optional<base::string16> clipboard_text_content_;
  base::Optional<gfx::Image> clipboard_image_content_;
  base::TimeDelta content_age_;
  bool suppress_content_;

  DISALLOW_COPY_AND_ASSIGN(FakeClipboardRecentContent);
};

#endif  // COMPONENTS_OPEN_FROM_CLIPBOARD_FAKE_CLIPBOARD_RECENT_CONTENT_H_
