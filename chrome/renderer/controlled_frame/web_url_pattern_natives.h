// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CONTROLLED_FRAME_WEB_URL_PATTERN_NATIVES_H_
#define CHROME_RENDERER_CONTROLLED_FRAME_WEB_URL_PATTERN_NATIVES_H_

#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8-forward.h"

namespace controlled_frame {

bool V8URLPatternToMatchPatterns(v8::Isolate* isolate,
                                 v8::Local<v8::Value>& input,
                                 std::string& out_match_pattern);

class WebUrlPatternNatives : public extensions::ObjectBackedNativeHandler {
 public:
  explicit WebUrlPatternNatives(extensions::ScriptContext* context);

  WebUrlPatternNatives(const WebUrlPatternNatives&) = delete;
  WebUrlPatternNatives& operator=(const WebUrlPatternNatives&) = delete;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  void URLPatternToMatchPatterns(
      const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace controlled_frame

#endif  // CHROME_RENDERER_CONTROLLED_FRAME_WEB_URL_PATTERN_NATIVES_H_
