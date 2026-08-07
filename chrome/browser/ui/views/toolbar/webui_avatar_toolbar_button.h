// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_AVATAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_AVATAR_TOOLBAR_BUTTON_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "components/browser_apis/ui_controllers/toolbar/icon_handle.h"
#include "ui/views/controls/button/button.h"

class AvatarToolbarButtonStateManager;
class WebUIToolbarControlDelegate;
class AvatarToolbarButtonTestAccessor;

// WebUIAvatarToolbarButton implements C++-side functionality for the
// WebUI-based implementation of the avatar button in the toolbar.
class WebUIAvatarToolbarButton : public AvatarToolbarButtonInterface {
 public:
  explicit WebUIAvatarToolbarButton(WebUIToolbarControlDelegate* delegate);
  WebUIAvatarToolbarButton(const WebUIAvatarToolbarButton&) = delete;
  WebUIAvatarToolbarButton& operator=(const WebUIAvatarToolbarButton&) = delete;
  ~WebUIAvatarToolbarButton() override;

  void Initialize();
  void SetAvatarButtonHovered(bool hovered);
  void SetAvatarButtonFocused(bool focused);

  // Returns whether an In-Product Help promo is currently showing for this
  // button.
  bool IsShowingIPHPromo() const;

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
  void SetAnnounceCallbackForTesting(
      base::OnceCallback<void(std::u16string)> callback) override;
  base::ScopedClosureRunner SetExplicitButtonState(
      const std::u16string& text,
      std::optional<std::u16string> accessibility_label,
      std::optional<base::RepeatingCallback<void(bool is_source_accelerator)>>
          explicit_action,
      bool should_announce) override;
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
  // Used by tests to access the private state_manager_ for verification and
  // fallback queries when the button is hidden.
  friend class ::AvatarToolbarButtonTestAccessor;
  void UpdateState();
  void UpdateAccessibilityLabel();
  void AnnounceInternal(std::u16string text);
  base::OnceCallback<void(std::u16string)> announce_callback_for_testing_;

  const raw_ptr<WebUIToolbarControlDelegate> delegate_;

  // May be null.
  std::unique_ptr<AvatarToolbarButtonStateManager> state_manager_;

  bool is_initialized_ = false;
  bool hovered_ = false;
  bool focused_ = false;
  bool is_showing_iph_promo_ = false;

  toolbar_ui_api::IconHandle avatar_icon_handle_;

  base::WeakPtrFactory<WebUIAvatarToolbarButton> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_AVATAR_TOOLBAR_BUTTON_H_
