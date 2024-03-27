// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_DELEGATE_H_

#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/base/models/image_model.h"

class Browser;
class Profile;
class AvatarToolbarButton;

namespace ui {
class ColorProvider;
}

// Internal structures.
namespace internal {
class StateManager;
enum class ButtonState;
}  // namespace internal

// Handles the business logic for AvatarToolbarButton.
// Listens to Chrome and Profile changes in order to compute the proper state of
// the button. This state is used to compute the information requested by
// the button to be shown, such as Text and color, Icon, tooltip text etc...
// The different states that can be reached:
// - Regular state: regular browsing session.
// - Private mode: Incognito or Guest browser sessions.
// - Identity name shown: the identity name is shown for a short period of time.
//   This can be triggered by identity changes in Chrome or when an IPH is
//   showing.
// - Explicit modifications override: such as displaying specific text when
//   intercept bubbles are displayed.
// - Sync paused/error state.
class AvatarToolbarButtonDelegate {
 public:
  AvatarToolbarButtonDelegate(AvatarToolbarButton* button, Browser* browser);

  AvatarToolbarButtonDelegate(const AvatarToolbarButtonDelegate&) = delete;
  AvatarToolbarButtonDelegate& operator=(const AvatarToolbarButtonDelegate&) =
      delete;

  ~AvatarToolbarButtonDelegate();

  // These info are based on the `ButtonState`.
  std::pair<std::u16string, std::optional<SkColor>> GetTextAndColor(
      const ui::ColorProvider* const color_provider) const;
  SkColor GetHighlightTextColor(
      const ui::ColorProvider* const color_provider) const;
  std::u16string GetAvatarTooltipText() const;
  std::pair<ChromeColorIds, ChromeColorIds> GetInkdropColors() const;
  ui::ImageModel GetAvatarIcon(int icon_size, SkColor icon_color) const;
  bool ShouldPaintBorder() const;

  [[nodiscard]] base::ScopedClosureRunner ShowExplicitText(
      const std::u16string& text);

  // Called by the AvatarToolbarButton to notify the delegate about events.
  void OnThemeChanged(const ui::ColorProvider* color_provider);

  // Overrides the duration of the avatar toolbar button text that is displayed
  // for a specific amount of time.
  static void SetTextDurationForTesting(base::TimeDelta duration);

 private:
  internal::ButtonState ComputeState() const;

  std::u16string GetProfileName() const;
  std::u16string GetShortProfileName() const;
  // Must only be called in states which have an avatar image (i.e. not
  // kGuestSession and not kIncognitoProfile).
  gfx::Image GetProfileAvatarImage(int preferred_size) const;
  // Returns the count of incognito or guest windows attached to the profile.
  int GetWindowCount() const;
  gfx::Image GetGaiaAccountImage() const;

  const raw_ptr<AvatarToolbarButton> avatar_toolbar_button_;
  const raw_ptr<Browser> browser_;
  const raw_ptr<Profile> profile_;

  // Text to be displayed while the state is
  // `ButtonState::kExplicitTextShowing`.
  // TODO(b/324018028): Move this info into it's own implementation of
  // StateProvider.
  std::u16string explicit_text_;

  std::unique_ptr<internal::StateManager> state_manager_;

  base::WeakPtrFactory<AvatarToolbarButtonDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_TOOLBAR_BUTTON_DELEGATE_H_
