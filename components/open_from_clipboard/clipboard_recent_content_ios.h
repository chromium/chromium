// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_IOS_H_
#define COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_IOS_H_

#include <string>

#include "base/time/time.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "url/gurl.h"

@class NSArray;
@class NSDate;
@class NSUserDefaults;
@class ClipboardRecentContentImplIOS;

// IOS implementation of ClipboardRecentContent.
// A large part of the implementation is in clipboard_recent_content_impl_ios,
// a GURL-free class that is used by some of the iOS extensions. Not using GURL
// in extensions is preferable as GURL requires depending on ICU which makes the
// extensions much larger.
class ClipboardRecentContentIOS : public ClipboardRecentContent {
 public:
  // |application_scheme| is the URL scheme that can be used to open the
  // current application, may be empty if no such scheme exists. Used to
  // determine whether or not the clipboard contains a relevant URL.
  // |group_user_defaults| is the NSUserDefaults used to store information on
  // pasteboard entry expiration. This information will be shared with other
  // application in the application group.
  ClipboardRecentContentIOS(const std::string& application_scheme,
                            NSUserDefaults* group_user_defaults,
                            bool only_use_clipboard_async);

  // Constructor that directly takes an |implementation|. For use in tests.
  explicit ClipboardRecentContentIOS(
      ClipboardRecentContentImplIOS* implementation);

  ClipboardRecentContentIOS(const ClipboardRecentContentIOS&) = delete;
  ClipboardRecentContentIOS& operator=(const ClipboardRecentContentIOS&) =
      delete;

  ~ClipboardRecentContentIOS() override;

  // ClipboardRecentContent implementation.
  std::optional<GURL> GetRecentURLFromClipboard() override;
  std::optional<std::u16string> GetRecentTextFromClipboard() override;
  std::optional<std::set<ClipboardContentType>> GetCachedClipboardContentTypes()
      override;
  void GetRecentImageFromClipboard(GetRecentImageCallback callback) override;
  bool HasRecentImageFromClipboard() override;
  void HasRecentContentFromClipboard(std::set<ClipboardContentType> types,
                                     HasDataCallback callback) override;
  void GetRecentURLFromClipboard(GetRecentURLCallback callback) override;
  void GetRecentTextFromClipboard(GetRecentTextCallback callback) override;
  base::TimeDelta GetClipboardContentAge() const override;
  void SuppressClipboardContent() override;
  void ClearClipboardContent() override;

 private:
  std::optional<gfx::Image> GetRecentImageFromClipboardInternal();
  void OnGetRecentImageFromClipboard(GetRecentImageCallback callback,
                                     const SkBitmap& sk_bitmap);

  // The implementation instance.
  __strong ClipboardRecentContentImplIOS* implementation_;
};

#endif  // COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_IOS_H_
