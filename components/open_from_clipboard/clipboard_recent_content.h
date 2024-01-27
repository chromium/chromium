// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_H_
#define COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

enum class ClipboardContentType { URL, Text, Image };

// Helper class returning an URL if the content of the clipboard can be turned
// into an URL, and if it estimates that the content of the clipboard is not too
// old.
class ClipboardRecentContent {
 public:
  ClipboardRecentContent();

  ClipboardRecentContent(const ClipboardRecentContent&) = delete;
  ClipboardRecentContent& operator=(const ClipboardRecentContent&) = delete;

  virtual ~ClipboardRecentContent();

  // Returns the global instance of the ClipboardRecentContent singleton. This
  // method does *not* create the singleton and will return null if no instance
  // was registered via SetInstance().
  static ClipboardRecentContent* GetInstance();

  // Sets the global instance of ClipboardRecentContent singleton.
  static void SetInstance(std::unique_ptr<ClipboardRecentContent> new_instance);

  // Returns clipboard content as URL, if it has a compatible type,
  // is recent enough, has not been suppressed and will not trigger a system
  // notification that the clipboard has been accessed.
  virtual std::optional<GURL> GetRecentURLFromClipboard() = 0;

  // Returns clipboard content as text, if it has a compatible type,
  // is recent enough, has not been suppressed and will not trigger a system
  // notification that the clipboard has been accessed.
  virtual std::optional<std::u16string> GetRecentTextFromClipboard() = 0;

  // Return if system's clipboard contains an image that will not trigger a
  // system notification that the clipboard has been accessed.
  virtual bool HasRecentImageFromClipboard() = 0;

  // Returns current clipboard content type(s) if it is recent enough and has
  // not been suppressed. This value will be nullopt during the brief period
  // when the clipboard is updating its cache. More succintly, this value will
  // be nullopt when the app is not sure what the latest clipboard contents are,
  // or when the value should not be returned due to the clipboard content's age
  // being too old. Differently, this value will be the non-nullopt empty set
  // when nothing is copied on the clipboard.
  //
  // Finally, this synchronous method slightly differs from the asynchronous
  // method HasRecentContentFromClipboard. This method synchronously returns the
  // ContentTypes being used given current pasteboard contents. Whereas
  // HasRecentContentFromClipboard exposes functionality to ask the application
  // if certain ContentTypes are being used on the clipboard, and retrieve a
  // response with the results.
  virtual std::optional<std::set<ClipboardContentType>>
  GetCachedClipboardContentTypes() = 0;

  /*
   On iOS, iOS 14 introduces new clipboard APIs that are async. The asynchronous
   forms of clipboard access below should be preferred.
   */
  using HasDataCallback =
      base::OnceCallback<void(std::set<ClipboardContentType>)>;
  using GetRecentURLCallback = base::OnceCallback<void(std::optional<GURL>)>;
  using GetRecentTextCallback =
      base::OnceCallback<void(std::optional<std::u16string>)>;
  using GetRecentImageCallback =
      base::OnceCallback<void(std::optional<gfx::Image>)>;

  // Returns whether the clipboard contains a URL to |HasDataCallback| if it
  // is recent enough and has not been suppressed.
  virtual void HasRecentContentFromClipboard(
      std::set<ClipboardContentType> types,
      HasDataCallback callback) = 0;

  // Returns clipboard content as URL to |GetRecentURLCallback|, if it has a
  // compatible type, is recent enough and has not been suppressed.
  virtual void GetRecentURLFromClipboard(GetRecentURLCallback callback) = 0;

  // Returns clipboard content as a string to |GetRecentTextCallback|, if it has
  // a compatible type, is recent enough and has not been suppressed.
  virtual void GetRecentTextFromClipboard(GetRecentTextCallback callback) = 0;

  // Returns clipboard content as image to |GetRecentImageCallback|, if it has a
  // compatible type, is recent enough and has not been suppressed.
  virtual void GetRecentImageFromClipboard(GetRecentImageCallback callback) = 0;

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
};

#endif  // COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_H_
