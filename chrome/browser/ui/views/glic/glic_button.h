// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BUTTON_H_

#include <concepts>
#include <variant>

#include "base/feature_list.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/glic/glic_button_interface.h"
#include "chrome/common/chrome_features.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/menus/simple_menu_model.h"

class TabStripController;

namespace views {
class LabelButton;
}

namespace glic {

inline constexpr int kIconSize = 16;
inline constexpr ui::ColorId kTextOnHighlight = ui::kColorSysOnPrimary;
inline constexpr ui::ColorId kForeground =
    kColorNewTabButtonForegroundFrameActive;
inline constexpr ui::ColorId kForegroundOnAltBackground =
    ui::kColorSysOnSurface;

bool EntrypointVariationsEnabled() {
  return base::FeatureList::IsEnabled(features::kGlicEntrypointVariations);
}

bool ShouldUseAltIcon() {
  return EntrypointVariationsEnabled() &&
         features::kGlicEntrypointVariationsAltIcon.Get();
}

const gfx::VectorIcon& GlicVectorIcon() {
  return GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON);
}

ui::ImageModel GetNormalIcon() {
  if (ShouldUseAltIcon()) {
    return ui::ImageModel::FromImageSkia(
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_GLIC_BUTTON_ALT_ICON));
  }
  return ui::ImageModel::FromVectorIcon(
      GlicVectorIcon(),
      ShouldUseAltIcon() ? kForegroundOnAltBackground : kForeground, kIconSize);
}

bool HighlightNudgeEnabled() {
  return EntrypointVariationsEnabled() &&
         features::kGlicEntrypointVariationsHighlightNudge.Get();
}

ui::ImageModel GetIconForHighlight() {
  if (!HighlightNudgeEnabled()) {
    return {};
  }
  return ui::ImageModel::FromVectorIcon(GlicVectorIcon(), kTextOnHighlight,
                                        kIconSize);
}

template <typename T>
  requires std::derived_from<T, views::LabelButton>
class GlicButton : public T,
                   public GlicButtonInterface,
                   public ui::SimpleMenuModel::Delegate {
 public:
  template <typename... BaseArgs>
  GlicButton(BrowserWindowInterface* browser_window_interface,
             std::optional<TabStripController*> tab_strip_controller,
             base::RepeatingClosure hovered_callback,
             base::RepeatingClosure mouse_down_callback,
             base::RepeatingClosure expansion_animation_done_callback,
             const std::u16string& tooltip,
             BaseArgs&&... base_args)
      : T(std::move(base_args)...),
        browser_window_interface_(browser_window_interface),
        menu_model_(CreateMenuModel()),
        hovered_callback_(std::move(hovered_callback)),
        mouse_down_callback_(std::move(mouse_down_callback)),
        expansion_animation_done_callback_(
            std::move(expansion_animation_done_callback)),
        normal_icon_(GetNormalIcon()),
        icon_for_highlight_(GetIconForHighlight()) {
    Init(tooltip);
  }

  GlicButton(const GlicButton&) = delete;
  GlicButton& operator=(const GlicButton&) = delete;
  ~GlicButton() override;

  // GlicButtonInterface:
  void SetDropToAttachIndicator(bool indicate) override;
  gfx::Rect GetBoundsWithInset() const override;

 protected:
  void Init(const std::u16string& tooltip);

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;

  // Model for the context menu.
  std::unique_ptr<ui::MenuModel> menu_model_;

  // Callback which is invoked when the button is hovered (i.e., the user is
  // more likely to interact with it soon).
  base::RepeatingClosure hovered_callback_;

  // Callback which is invoked when there is a mouse down event on the button
  // (i.e., the user is very likely to interact with it soon).
  base::RepeatingClosure mouse_down_callback_;

  // Invoked when the button hide animation finishes.
  base::RepeatingClosure expansion_animation_done_callback_;

  const ui::ImageModel normal_icon_;
  const ui::ImageModel icon_for_highlight_;

 private:
  // Creates the model for the context menu.
  std::unique_ptr<ui::SimpleMenuModel> CreateMenuModel();

  virtual void SetBackgroundFrameActiveColorId(ui::ColorId new_color_id) {}

  base::WeakPtrFactory<GlicButton> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BUTTON_H_
