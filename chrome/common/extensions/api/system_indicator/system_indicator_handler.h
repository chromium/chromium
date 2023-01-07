// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_HANDLER_H_

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

class ExtensionIconSet;

namespace extensions {

// Parses the "system_indicator" manifest key.
class SystemIndicatorHandler : public ManifestHandler {
 public:
  SystemIndicatorHandler();

  SystemIndicatorHandler(const SystemIndicatorHandler&) = delete;
  SystemIndicatorHandler& operator=(const SystemIndicatorHandler&) = delete;

  ~SystemIndicatorHandler() override;

  // Returns the default system indicator icon for the given |extension|, if
  // the extension has a system indicator, and null otherwise. Note that if the
  // extension has a system indicator, the result is never null (though the
  // set may be empty).
  static const ExtensionIconSet* GetSystemIndicatorIcon(
      const Extension& extension);

  // ManifestHandler:
  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_HANDLER_H_
