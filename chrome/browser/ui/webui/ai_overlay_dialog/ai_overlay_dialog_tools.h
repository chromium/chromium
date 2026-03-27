// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_TOOLS_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_TOOLS_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/values.h"

// Represents the native interface in the Browser Process.
// Handles high-privilege actions like tab management and navigation.
//
// Schema for the built-in global browser-level tools.
// This interface is parsed by generate_tool_definitions.py to produce the
// tool definitions passed to the Gemini Live API.
class AiOverlayDialogTools {
 public:
  virtual ~AiOverlayDialogTools() = default;

  // --- AI OVERLAY TOOLS START ---
  // Open a URL.
  virtual void OpenUrl(const std::string& url,
                       bool new_tab,
                       base::OnceCallback<void(bool)> callback) = 0;
  // Search using the default search engine.
  virtual void PerformSearch(const std::string& query,
                             bool new_tab,
                             base::OnceCallback<void(bool)> callback) = 0;
  // Switch to another open tab by fuzzy matching name or URL.
  virtual void SwitchTab(
      const std::string& query,
      base::OnceCallback<void(base::DictValue)> callback) = 0;
  // Close the current browser tab.
  virtual void CloseCurrentTab(base::OnceCallback<void(bool)> callback) = 0;
  // Go back to the previous page in history.
  virtual void GoBack(base::OnceCallback<void(bool)> callback) = 0;
  // Go forward to the next page in history.
  virtual void GoForward(base::OnceCallback<void(bool)> callback) = 0;
  // Refresh the current page.
  virtual void ReloadPage(base::OnceCallback<void(bool)> callback) = 0;
  // Find and highlight text on the current page.
  virtual void FindAndHighlight(
      const std::string& query,
      base::OnceCallback<void(base::DictValue)> callback) = 0;
  // Scroll the page.
  virtual void Scroll(const std::string& direction,
                      double magnitude,
                      base::OnceCallback<void(bool)> callback) = 0;
  // Play the video on the current page.
  virtual void PlayVideo(base::OnceCallback<void(bool)> callback) = 0;
  // Pause the video on the current page.
  virtual void PauseVideo(base::OnceCallback<void(bool)> callback) = 0;
  // Seek the video to a specific timecode.
  virtual void SeekToTimestamp(const std::string& timecode,
                               base::OnceCallback<void(bool)> callback) = 0;
  // --- AI OVERLAY TOOLS END ---
};

#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_TOOLS_H_
