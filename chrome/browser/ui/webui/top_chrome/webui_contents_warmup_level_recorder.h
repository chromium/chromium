// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_WARMUP_LEVEL_RECORDER_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_WARMUP_LEVEL_RECORDER_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_warmup_level.h"

namespace content {

class RenderProcessHost;
class WebContents;

}  // namespace content

// This struct captures the state immediately before a WebUI bubble is created.
struct WebUIContentsWarmupLevelPreCondition {
  WebUIContentsWarmupLevelPreCondition();
  WebUIContentsWarmupLevelPreCondition& operator=(
      WebUIContentsWarmupLevelPreCondition&&);
  ~WebUIContentsWarmupLevelPreCondition();
  // The IDs of the spare processes before the contents is created. This can be
  // empty when the memory pressure is high or a spare process creation is
  // delayed.
  std::vector<int> spare_process_ids;

  // Use WeakPtr because the preloaded contents could be destroyed, e.g. after
  // a request of contents under a different browser context.
  base::WeakPtr<content::WebContents> preloaded_contents;

  // The RenderProcessHost of `preloaded_contents`, if exists.
  raw_ptr<const content::RenderProcessHost> preloaded_process = nullptr;

  // The preloaded WebUI's host URL, if exists.
  std::string preloaded_host;
};

class WebUIContentsWarmupLevelRecorder {
 public:
  WebUIContentsWarmupLevelRecorder();
  ~WebUIContentsWarmupLevelRecorder();

  void BeforeContentsCreation();
  void AfterContentsCreation(content::WebContents* web_contents);

  void SetUsedCachedContents(bool used_cached_contents);

  WebUIContentsWarmupLevel GetWarmupLevel() const;

 private:
  std::optional<WebUIContentsWarmupLevelPreCondition> pre_condition_;
  std::optional<WebUIContentsWarmupLevel> level_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_WARMUP_LEVEL_RECORDER_H_
