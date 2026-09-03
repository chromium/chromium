// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_WAAP_UTILS_H_
#define CHROME_BROWSER_UI_WAAP_WAAP_UTILS_H_

#include <memory>

#include "base/time/time.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}

namespace waap {

// Represents the source of a new browser window creation.
enum class NewWindowCreationSource {
  kUnknown = 0,
  kBrowserInitiated = 1,
  kDragToNewWindow = 2,
  kSessionRestore = 3,
  kMaxValue = kSessionRestore,
};

// Result of initial surface synchronization for the WebUI toolbar.
// LINT.IfChange(InitialWebUISurfaceSyncResult)
enum class InitialWebUISurfaceSyncResult {
  kReadyWithinDeadline = 0,
  kDeadlineExceededPaintedLater = 1,
  kDeadlineExceededClosedBeforePaint = 2,
  kDeadlineExceededRenderProcessGone = 3,
  kClosedBeforeBrowserPresentation = 4,
  kRenderProcessGoneBeforeBrowserPresentation = 5,
  kMaxValue = kRenderProcessGoneBeforeBrowserPresentation,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ui/enums.xml:InitialWebUISurfaceSyncResult)

// Returns true if the given URL is the initial WebUI scheme.
// This is only relevant on non-Android platforms.
bool IsForInitialWebUI(const GURL& url);

class PrewarmHelper {
 public:
  // Configures the WebContents used for the initial WebUI (e.g. page load
  // metrics, background color, zoom gestures, and color provider source).
  static void ConfigureWebUIContents(content::WebContents* web_contents,
                                     Profile* profile);

  // Prewarms the WebUI toolbar WebContents for the given profile.
  // Creates the WebContents and configures it using ConfigureWebUIContents.
  // If `pre_navigate` is true, starts loading the toolbar URL; otherwise, only
  // initializes the renderer process.
  static std::unique_ptr<content::WebContents> PrewarmWebUIContents(
      Profile* profile,
      bool pre_navigate);
};

}  // namespace waap

#endif  // CHROME_BROWSER_UI_WAAP_WAAP_UTILS_H_
