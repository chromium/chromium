// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_AVATAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_AVATAR_TOOLBAR_BUTTON_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"

class Browser;
class AvatarToolbarButtonStateManager;
class WebUIToolbarWebView;

// WebUIAvatarToolbarButton implements C++-side functionality for the
// WebUI-based implementation of the avatar button in the toolbar.
class WebUIAvatarToolbarButton : public AvatarToolbarButtonInterface {
 public:
  WebUIAvatarToolbarButton(WebUIToolbarWebView* webui_toolbar_web_view,
                           Browser* browser);
  WebUIAvatarToolbarButton(const WebUIAvatarToolbarButton&) = delete;
  WebUIAvatarToolbarButton& operator=(const WebUIAvatarToolbarButton&) = delete;
  ~WebUIAvatarToolbarButton() override;

  void Initialize();

  // AvatarToolbarButtonInterface overrides:
  void UpdateIcon() override;
  void UpdateText() override;
  bool IsMouseHovered() const override;
  bool HasFocus() const override;
  views::DialogDelegate* GetDialogDelegate() override;
  void AddObserver(AvatarToolbarButtonInterface::Observer* observer) override;
  void RemoveObserver(
      AvatarToolbarButtonInterface::Observer* observer) override;
  void ButtonPressed(bool is_source_accelerator) override;
  base::ScopedClosureRunner SetExplicitButtonState(
      const std::u16string& text,
      std::optional<std::u16string> accessibility_label,
      std::optional<base::RepeatingCallback<void(bool is_source_accelerator)>>
          explicit_action) override;
  bool HasExplicitButtonState() const override;
  void MaybeShowProfileSwitchIPH() override;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  void MaybeShowSupervisedUserSignInIPH() override;
  void MaybeShowSignInBenefitsIPH() override;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  void ClearActiveStateForTesting() override;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void ForceShowingPromoForTesting() override;
  bool GetStateAndFireSignedOutTriggerDelayTimerForTesting() override;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  bool is_initialized() const { return is_initialized_; }

  void NotifyIPHPromoChanged(bool has_promo);

 private:
  void UpdateState();
  void UpdateAccessibilityLabel();

  const raw_ptr<WebUIToolbarWebView> webui_toolbar_web_view_;

  // May be null.
  std::unique_ptr<AvatarToolbarButtonStateManager> state_manager_;
  std::u16string accessibility_name_;
  std::u16string accessibility_description_;

  bool is_initialized_ = false;

  base::WeakPtrFactory<WebUIAvatarToolbarButton> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_AVATAR_TOOLBAR_BUTTON_H_
