// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_NATIVELY_CONNECTABLE_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_NATIVELY_CONNECTABLE_HANDLER_H_

#include <set>
#include <string>

#include "base/containers/span.h"
#include "base/macros.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// A structure to hold the parsed list of native messaging hosts that can
// connect to this extension.
struct NativelyConnectableHosts : public Extension::ManifestData {
  NativelyConnectableHosts();
  ~NativelyConnectableHosts() override;

  static const std::set<std::string>* GetConnectableNativeMessageHosts(
      const Extension& extension);

  // A set of native messaging hosts allowed to initiate connection to this
  // extension.
  std::set<std::string> hosts;
};

// Parses the "natively_connectable" manifest key.
class NativelyConnectableHandler : public ManifestHandler {
 public:
  NativelyConnectableHandler();
  ~NativelyConnectableHandler() override;

  bool Parse(Extension* extension, base::string16* error) override;

 private:
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(NativelyConnectableHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_NATIVELY_CONNECTABLE_HANDLER_H_
