// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_PROMPT_MANAGER_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_PROMPT_MANAGER_H_

#include <memory>

#include "base/memory/singleton.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_surface_manager.h"

// DefaultBrowserPromptManager is a Global singleton class that is responsible
// for owning and displaying prompts that nudge user to set Chrome as their
// default browser.
class DefaultBrowserPromptManager {
 public:
  DefaultBrowserPromptManager(const DefaultBrowserPromptManager&) = delete;
  DefaultBrowserPromptManager& operator=(const DefaultBrowserPromptManager&) =
      delete;

  enum class CloseReason {
    kAccept,
    kDismiss,
  };

  static DefaultBrowserPromptManager* GetInstance();

  bool show_app_menu_item() const { return show_app_menu_item_; }

  // Returns true if the prompt was shown, false if not.
  bool MaybeShowPrompt();

  void ShowPrompts(bool can_pin_to_taskbar);
  void CloseAllPrompts(CloseReason close_reason);

 private:
  friend struct base::DefaultSingletonTraits<DefaultBrowserPromptManager>;

  DefaultBrowserPromptManager();
  ~DefaultBrowserPromptManager();

  // This will trigger the showing of the info bar.
  void OnCanPinToTaskbarResult(bool should_offer_to_pin);

  void SetAppMenuItemVisibility(bool show);

  bool show_app_menu_item_ = false;

  // The manager responsible for the UI surface of the default browser prompt.
  // This can vary (e.g., Infobar vs. Bubble) based on configuration.
  std::unique_ptr<DefaultBrowserSurfaceManager> prompt_surface_manager_;
};

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_PROMPT_MANAGER_H_
