// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_DIRECTORY_KEY_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_DIRECTORY_KEY_H_

#include <string>
#include <string_view>

namespace paint_preview {

// A key for a specific subdirectory (== captured paint preview).
class DirectoryKey {
 public:
  DirectoryKey() = default;
  explicit DirectoryKey(std::string_view ascii_dirname)
      : ascii_dirname_(ascii_dirname) {}
  ~DirectoryKey() = default;

  const std::string& AsciiDirname() const { return ascii_dirname_; }

 private:
  std::string ascii_dirname_;
};

bool operator<(const DirectoryKey& a, const DirectoryKey& b);

bool operator==(const DirectoryKey& a, const DirectoryKey& b);

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_DIRECTORY_KEY_H_
