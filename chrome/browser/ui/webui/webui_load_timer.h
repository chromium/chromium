// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_LOAD_TIMER_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_LOAD_TIMER_H_

#include <memory>
#include <string>

#include "content/public/browser/web_contents_observer.h"

namespace base {
class ElapsedTimer;
}

// Measures and reports page load times for WebUI.
class WebuiLoadTimer : public content::WebContentsObserver {
 public:
  // Load times are reported to UMA using the provided strings which
  // must not be empty.
  // * |document_initial_load_uma_id| - corresponds to DOMContentLoaded
  // * |document_load_completed_uma_id| - corresponds to
  //   DocumentOnLoadCompletedInPrimaryMainFrame
  WebuiLoadTimer(content::WebContents* web_contents,
                 const std::string& document_initial_load_uma_id,
                 const std::string& document_load_completed_uma_id);

  WebuiLoadTimer(const WebuiLoadTimer&) = delete;
  WebuiLoadTimer& operator=(const WebuiLoadTimer&) = delete;

  ~WebuiLoadTimer() override;

  // WebContentsObserver
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

 private:
  std::string document_initial_load_uma_id_;
  std::string document_load_completed_uma_id_;
  std::unique_ptr<base::ElapsedTimer> timer_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_LOAD_TIMER_H_
