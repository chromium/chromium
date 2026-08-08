// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "components/prefs/pref_member.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/gfx/native_ui_types.h"

class Profile;

namespace omnibox_everywhere {

class OmniboxEverywhereBackgroundModeManager;

// The source of the Omnibox Everywhere invocation.
enum class InvocationSource {
  // Triggered by a global system hotkey registration.
  kGlobalHotkey,
  // Triggered by the profile picker.
  kProfilePicker,
  // Triggered from the status tray/menu bar icon.
  kStatusTrayIcon,
};

// Coordinator class that manages the Omnibox Everywhere desktop feature.
// Exists as a process-global singleton owned by GlobalFeatures.
class OmniboxEverywhereController
    : public ui::GlobalAcceleratorListener::Observer {
 public:
  explicit OmniboxEverywhereController(
      OmniboxEverywhereUIManager::ContentsWrapperFactory
          contents_wrapper_factory = {},
      ui::GlobalAcceleratorListener* listener = nullptr);
  OmniboxEverywhereController(const OmniboxEverywhereController&) = delete;
  OmniboxEverywhereController& operator=(const OmniboxEverywhereController&) =
      delete;
  ~OmniboxEverywhereController() override;

  // Called when the Omnibox Everywhere is invoked via one of the entry points.
  void OnInvoke(InvocationSource source,
                Profile* profile,
                gfx::NativeWindow context = gfx::NativeWindow());

  OmniboxEverywhereUIManager* ui_manager() { return ui_manager_.get(); }
  const OmniboxEverywhereUIManager* ui_manager() const {
    return ui_manager_.get();
  }

  // Closes the Omnibox Everywhere widget if it is open.
  void Close();

  // Returns true if the Omnibox Everywhere widget is visible.
  bool IsVisible() const;

  // Shows Chrome's native Profile Picker to switch profiles for Omnibox
  // Everywhere.
  void ShowProfilePicker();

  // Called during profile teardown to synchronously close the widget.
  void ShutdownForProfile(Profile* profile);

  // ui::GlobalAcceleratorListener::Observer:
  void OnKeyPressed(const ui::Accelerator& accelerator) override;
  void ExecuteCommand(const std::string& accelerator_group_id,
                      const std::string& command_id) override;

 private:
  void OnStatusIconClicked();
  void OnProfilePicked(Profile* new_profile);
  void InvokeForActiveBrowserProfile(InvocationSource source);

  // Resolves the target profile for the Omnibox Everywhere invocation.
  // TODO(crbug.com/527183107): Implement a better profile selection heuristic.
  Profile* GetTargetProfile();

  // Registers or unregisters the global hotkey accelerator according to feature
  // flag and preference settings.
  void UpdateHotkeyRegistration();

  BooleanPrefMember hotkey_pref_member_;
  std::unique_ptr<OmniboxEverywhereUIManager> ui_manager_;
  std::unique_ptr<OmniboxEverywhereBackgroundModeManager>
      background_mode_manager_;
  raw_ptr<ui::GlobalAcceleratorListener> listener_ = nullptr;
  base::WeakPtrFactory<OmniboxEverywhereController> weak_factory_{this};
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_CONTROLLER_H_
