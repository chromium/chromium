// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_API_PLATFORM_KEYS_NATIVES_H_
#define CHROME_RENDERER_EXTENSIONS_API_PLATFORM_KEYS_NATIVES_H_

#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8.h"

namespace extensions {
class ScriptContext;

class PlatformKeysNatives : public ObjectBackedNativeHandler {
 public:
  explicit PlatformKeysNatives(ScriptContext* context);

  PlatformKeysNatives(const PlatformKeysNatives&) = delete;
  PlatformKeysNatives& operator=(const PlatformKeysNatives&) = delete;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  // Normalizes algorithm parameters, and then validates the expected parameters
  // for the algorithm/operation combination.
  // Expects the following arguments in |call_info|:
  //   |parameters|: An object containing the parameters to normalize.
  //   |operation|: A string describing the operation. Supported operations are
  //     "ImportKey", "GenerateKey", "Sign" and "Verify".
  // Returns the normalized dictionary on success, or null if some required
  // parameters are missing or not supported. Note that it returns untyped
  // arrays instead of typed arrays (e.g. for RSA publicExponent).
  void NormalizeAlgorithm(const v8::FunctionCallbackInfo<v8::Value>& call_info);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_API_PLATFORM_KEYS_NATIVES_H_
