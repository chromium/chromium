// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_EXTENSIONS_RENDERER_CLIENT_H_
#define CHROMECAST_RENDERER_CAST_EXTENSIONS_RENDERER_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "extensions/renderer/extensions_renderer_client.h"

namespace extensions {
class Dispatcher;

class CastExtensionsRendererClient : public ExtensionsRendererClient {
 public:
  CastExtensionsRendererClient();
  ~CastExtensionsRendererClient() override;

  // ExtensionsRendererClient implementation.
  bool IsIncognitoProcess() const override;
  int GetLowestIsolatedWorldId() const override;
  Dispatcher* GetDispatcher() override;
  bool ExtensionAPIEnabledForServiceWorkerScript(
      const GURL& scope,
      const GURL& script_url) const override;

 private:
  std::unique_ptr<Dispatcher> dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(CastExtensionsRendererClient);
};

}  // namespace extensions

#endif  // CHROMECAST_RENDERER_CAST_EXTENSIONS_RENDERER_CLIENT_H_
