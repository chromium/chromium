// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_MINIMUM_CHROME_VERSION_CHECKER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_MINIMUM_CHROME_VERSION_CHECKER_H_

#include "extensions/common/manifest_handler.h"

namespace extensions {

// Checks that the "minimum_chrome_version" requirement is met.
class MinimumChromeVersionChecker : public ManifestHandler {
 public:
  MinimumChromeVersionChecker();

  MinimumChromeVersionChecker(const MinimumChromeVersionChecker&) = delete;
  MinimumChromeVersionChecker& operator=(const MinimumChromeVersionChecker&) =
      delete;

  ~MinimumChromeVersionChecker() override;

  // Validate minimum Chrome version. We don't need to store this, since the
  // extension is not valid if it is incorrect.
  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_MINIMUM_CHROME_VERSION_CHECKER_H_
