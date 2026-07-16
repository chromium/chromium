// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_CONTROLLER_H_

#include <memory>

class Profile;

namespace omnibox_everywhere {

class OmniboxEverywhereUIManager;

// The source of the Omnibox Everywhere invocation.
enum class InvocationSource {
  // Triggered by a global system hotkey registration.
  kGlobalHotkey,
};

// Coordinator class that manages the Omnibox Everywhere desktop feature.
// Exists as a process-global singleton owned by GlobalFeatures.
class OmniboxEverywhereController {
 public:
  OmniboxEverywhereController();
  OmniboxEverywhereController(const OmniboxEverywhereController&) = delete;
  OmniboxEverywhereController& operator=(const OmniboxEverywhereController&) =
      delete;
  ~OmniboxEverywhereController();

  // Called when the Omnibox Everywhere is invoked via one of the entry points.
  void OnInvoke(InvocationSource source, Profile* profile);

  OmniboxEverywhereUIManager* ui_manager() { return ui_manager_.get(); }
  const OmniboxEverywhereUIManager* ui_manager() const {
    return ui_manager_.get();
  }

  // Closes the Omnibox Everywhere widget if it is open.
  void Close();

  // Returns true if the Omnibox Everywhere widget is visible.
  bool IsVisible() const;

  // Called during profile teardown to synchronously close the widget.
  void ShutdownForProfile(Profile* profile);

 private:
  std::unique_ptr<OmniboxEverywhereUIManager> ui_manager_;
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_CONTROLLER_H_
