// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_WARMUP_LEVEL_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_WARMUP_LEVEL_H_

#include <string>

// The readiness levels of the browser prior to showing a new WebUI bubble,
// ordered by increasing readiness. Higher levels have lower latency at the cost
// of greater memory use.
enum class WebUIContentsWarmupLevel {
  // No render process is available. No pre-existing process, including spare
  // renderers, can be used or reused for this WebUI.
  kNoRenderer,
  // Uses a spare render process for this WebUI.
  kSpareRenderer,
  // Uses a render process that already hosts other WebUIs prior to this WebUI.
  kDedicatedRenderer,
  // Uses a WebContents that is preloaded with a different WebUI and therefore
  // requires redirection to this WebUI.
  // TODO(325836830): WebContents forbids navigation between WebUIs. This
  // is mostly due to the asynchronous update of RenderFrameHost that causes
  // difficulty in WebUIImpl lifetime management.
  kRedirectedWebContents,
  // Uses a WebContents that is preloaded with this WebUI but is never shown.
  // TODO(325928324): investigate why this is slower than re-showing contents.
  kPreloadedWebContents,
  // Re-showing a WebContents that has already navigated to this WebUI.
  kReshowingWebContents,
};

std::string ToString(WebUIContentsWarmupLevel warmup_level);

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_WARMUP_LEVEL_H_
