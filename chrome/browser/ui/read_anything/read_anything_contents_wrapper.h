// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTENTS_WRAPPER_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTENTS_WRAPPER_H_

#include <memory>

#include "base/check.h"

class ReadAnythingUntrustedUI;

template <typename T>
class WebUIContentsWrapperT;

// Allows for the passing of a `WebUIContentsWrapperT<ReadAnythingUntrustedUI>`
// without having to (directly or indirectly) include either class. This avoids
// circular includes with the WebUI project.
//
// Public .h files in this project may only expose this class, since they may
// not include `ReadAnythingUntrustedUI` directly.
//
// Behaves more or less as a unique_ptr; pass by value.
class ReadAnythingContentsWrapper {
 public:
  using Ptr = std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>;

  ReadAnythingContentsWrapper();
  explicit ReadAnythingContentsWrapper(Ptr wrapper);
  ReadAnythingContentsWrapper(ReadAnythingContentsWrapper&&) noexcept;
  ReadAnythingContentsWrapper& operator=(
      ReadAnythingContentsWrapper&&) noexcept;
  ~ReadAnythingContentsWrapper();

  explicit operator bool() const { return !!wrapper_; }

  bool operator!() const { return !wrapper_; }

  WebUIContentsWrapperT<ReadAnythingUntrustedUI>* operator->() const {
    CHECK(wrapper_);
    return wrapper_.get();
  }

  WebUIContentsWrapperT<ReadAnythingUntrustedUI>& operator*() const {
    CHECK(wrapper_);
    return *wrapper_;
  }

  Ptr release() && { return std::move(wrapper_); }

 private:
  Ptr wrapper_;
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_CONTENTS_WRAPPER_H_
