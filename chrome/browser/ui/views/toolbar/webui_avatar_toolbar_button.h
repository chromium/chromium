// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_AVATAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_AVATAR_TOOLBAR_BUTTON_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "components/browser_apis/ui_controllers/toolbar/icon_handle.h"
#include "ui/views/controls/button/button.h"

class AvatarToolbarButtonStateManager;
class WebUIToolbarControlDelegate;
class AvatarToolbarButtonTestAccessor;

namespace ui {
class TrackedElement;
}

// WebUIAvatarToolbarButton implements C++-side functionality for the
// WebUI-based implementation of the avatar button in the toolbar.
//
// ButtonPressed() makes the button visible if it has overflowed, and keeps it
// visible while any resulting bubble is open.
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
  bool IsReadyForIPH() const override;

  void ClearActiveStateForTesting() override;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  void ForceShowingPromoForTesting() override;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  bool is_initialized() const { return is_initialized_; }

  void NotifyIPHPromoChanged(bool has_promo);

  // Called when the toolbar state's `controls_appear_inactive` changes, so the
  // icon can be updated.
  void OnControlsAppearInactiveChanged();

 private:
  // Used by tests to access the private state_manager_ for verification and
  // fallback queries when the button is hidden.
  friend class ::AvatarToolbarButtonTestAccessor;

  // Callback invoked by RunWhenVisible(). `keep_visible` keeps the button from
  // being hidden due to toolbar overflow as long as it is alive. The callee
  // should keep it alive for as long as it needs the button to be visible
  // (e.g., while a bubble anchored to it is open), and destroy it as soon as it
  // no longer does.
  using WhenVisibleCallback =
      base::OnceCallback<void(base::ScopedClosureRunner keep_visible)>;

  void UpdateState();
  void UpdateAccessibilityLabel();
  void AnnounceInternal(std::u16string text);

  // Invokes `callback` once the button is visible. If the button is already
  // visible, `callback` is invoked synchronously. Otherwise, the button is
  // prevented from overflowing, and `callback` is invoked once it is visible.
  // If `this` is destroyed first, `callback` is never invoked. If the button is
  // never shown for the profile, `callback` is invoked synchronously with an
  // empty `keep_visible`.
  //
  // TODO(crbug.com/556290451): Expose this via AvatarToolbarButtonInterface,
  // so other code that anchors bubbles to the avatar button can use it.
  void RunWhenVisible(WhenVisibleCallback callback);

  // Returns a closure that keeps the button from being hidden due to overflow
  // as long as it is alive. Multiple closures may be outstanding at once.
  base::ScopedClosureRunner PreventOverflow();
  void OnPreventOverflowReleased();

  // Returns the button's TrackedElement, or nullptr if the button is not
  // currently visible.
  ui::TrackedElement* GetVisibleButtonElement();

  // Called when the button becomes visible while there are pending
  // RunWhenVisible() calls.
  void OnButtonShown(ui::TrackedElement* element);

  // Handles a button press once the button is visible.
  void OnButtonPressedAndVisible(bool is_source_accelerator,
                                 base::ScopedClosureRunner keep_visible);

  base::OnceCallback<void(std::u16string)> announce_callback_for_testing_;

  const raw_ptr<WebUIToolbarControlDelegate> delegate_;

  // May be null.
  std::unique_ptr<AvatarToolbarButtonStateManager> state_manager_;

  bool is_initialized_ = false;
  bool hovered_ = false;
  bool focused_ = false;
  bool is_showing_iph_promo_ = false;

  // Number of outstanding PreventOverflow() closures. While non-zero,
  // `prevent_overflow` is set in the state sent to the WebUI.
  int prevent_overflow_count_ = 0;

  // RunWhenVisible() callbacks waiting for the button to be shown, each with
  // its `keep_visible` token already bound.
  std::vector<base::OnceClosure> pending_callbacks_;
  // Non-null only when `pending_callbacks_` is non-empty.
  base::CallbackListSubscription button_shown_subscription_;

  toolbar_ui_api::IconHandle avatar_icon_handle_;

  base::WeakPtrFactory<WebUIAvatarToolbarButton> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_AVATAR_TOOLBAR_BUTTON_H_
