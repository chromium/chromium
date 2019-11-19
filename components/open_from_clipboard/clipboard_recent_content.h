// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_H_
#define COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

// Helper class returning an URL if the content of the clipboard can be turned
// into an URL, and if it estimates that the content of the clipboard is not too
// old.
class ClipboardRecentContent {
 public:
  ClipboardRecentContent();
  virtual ~ClipboardRecentContent();

  // Returns the global instance of the ClipboardRecentContent singleton. This
  // method does *not* create the singleton and will return null if no instance
  // was registered via SetInstance().
  static ClipboardRecentContent* GetInstance();

  // Sets the global instance of ClipboardRecentContent singleton.
  static void SetInstance(std::unique_ptr<ClipboardRecentContent> new_instance);

  // Returns clipboard content as URL, if it has a compatible type,
  // is recent enough and has not been suppressed.
  virtual base::Optional<GURL> GetRecentURLFromClipboard() = 0;

  // Returns clipboard content as text, if it has a compatible type,
  // is recent enough and has not been suppressed.
  virtual base::Optional<base::string16> GetRecentTextFromClipboard() = 0;

  // Returns clipboard content as image, if it has a compatible type,
  // is recent enough and has not been suppressed.
  virtual base::Optional<gfx::Image> GetRecentImageFromClipboard() = 0;

  // Returns how old the content of the clipboard is.
  virtual base::TimeDelta GetClipboardContentAge() const = 0;

  // Prevent GetRecentURLFromClipboard from returning anything until the
  // clipboard's content changed.
  virtual void SuppressClipboardContent() = 0;

  // Clear clipboard content. Different with |SuppressClipboardContent|, this
  // function will clear content in the clipboard.
  virtual void ClearClipboardContent() = 0;

 protected:
  // GetRecentURLFromClipboard() should never return a URL from a clipboard
  // older than this.
  static base::TimeDelta MaximumAgeOfClipboard();

 private:
  DISALLOW_COPY_AND_ASSIGN(ClipboardRecentContent);
};

#endif  // COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_H_
