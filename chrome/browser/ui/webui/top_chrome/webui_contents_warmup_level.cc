// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_contents_warmup_level.h"

#include <string>

#include "base/notimplemented.h"

std::string ToString(WebUIContentsWarmupLevel warmup_level) {
  switch (warmup_level) {
    case WebUIContentsWarmupLevel::kNoRenderer:
      return "NoRenderer";
    case WebUIContentsWarmupLevel::kSpareRenderer:
      return "SpareRenderer";
    case WebUIContentsWarmupLevel::kDedicatedRenderer:
      return "DedicatedRenderer";
    case WebUIContentsWarmupLevel::kRedirectedWebContents:
      return "RedirectedWebContents";
    case WebUIContentsWarmupLevel::kPreloadedWebContents:
      return "PreloadedWebContents";
    case WebUIContentsWarmupLevel::kReshowingWebContents:
      return "ReshowingWebContents";
    default:
      NOTIMPLEMENTED();
      return "";
  }
}
