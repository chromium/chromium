
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/toolbar/toolbar_glic_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/glic/glic_button.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/ink_drop.h"

namespace {
constexpr int kCloseButtonSize = 16;
}  // namespace

namespace glic {

ToolbarGlicButton::ToolbarGlicButton(
    BrowserWindowInterface* browser_window_interface,
    base::RepeatingClosure hovered_callback,
    base::RepeatingClosure mouse_down_callback,
    base::RepeatingClosure expansion_animation_done_callback,
    const std::u16string& tooltip,
    PressedCallback pressed_callback)
    : GlicButton<ToolbarButton>(browser_window_interface,
                                hovered_callback,
                                mouse_down_callback,
                                expansion_animation_done_callback,
                                tooltip,
                                pressed_callback) {}

ToolbarGlicButton::~ToolbarGlicButton() = default;

void ToolbarGlicButton::AddedToWidget() {
  SetDefaultBackgroundColorId(kColorToolbarGlicButtonBackgroundDefault);
  GlicButton<ToolbarButton>::AddedToWidget();
}

bool ToolbarGlicButton::IsWidgetAlive() const {
  const views::Widget* widget = GetWidget();
  return widget && !widget->IsClosed();
}

void ToolbarGlicButton::SetForegroundFrameActiveColorId(
    ui::ColorId new_color_id) {
  UpdateColors();
}
void ToolbarGlicButton::SetForegroundFrameInactiveColorId(
    ui::ColorId new_color_id) {
  UpdateColors();
}
void ToolbarGlicButton::SetBackgroundFrameActiveColorId(
    ui::ColorId new_color_id) {
  UpdateColors();
}
void ToolbarGlicButton::SetBackgroundFrameInactiveColorId(
    ui::ColorId new_color_id) {
  UpdateColors();
}

void ToolbarGlicButton::UpdateColors() {
  ToolbarButton::UpdateColorsAndInsets();
}

void ToolbarGlicButton::SetCloseButtonFocusBehavior(
    views::View::FocusBehavior focus_behavior) {}

void ToolbarGlicButton::AddCloseButton(PressedCallback pressed_callback) {
  auto close_button =
      std::make_unique<views::LabelButton>(std::move(pressed_callback));
  close_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ORGANIZE_CLOSE));

  const ui::ImageModel icon_image_model = ui::ImageModel::FromVectorIcon(
      vector_icons::kCloseChromeRefreshIcon,
      kColorTabSearchButtonCRForegroundFrameActive, kCloseButtonSize);

  close_button->SetImageModel(views::Button::STATE_NORMAL, icon_image_model);
  close_button->SetImageModel(views::Button::STATE_HOVERED, icon_image_model);
  close_button->SetImageModel(views::Button::STATE_PRESSED, icon_image_model);

  close_button->SetPaintToLayer();
  close_button->layer()->SetFillsBoundsOpaquely(false);

  views::InkDrop::Get(close_button.get())
      ->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(close_button.get())->SetHighlightOpacity(0.16f);
  views::InkDrop::Get(close_button.get())->SetVisibleOpacity(0.14f);
  views::InkDrop::Get(close_button.get())
      ->SetBaseColor(kColorTabSearchButtonCRForegroundFrameActive);

  auto ink_drop_highlight_path =
      std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets());
  views::HighlightPathGenerator::Install(close_button.get(),
                                         std::move(ink_drop_highlight_path));

  close_button->SetPreferredSize(gfx::Size(kCloseButtonSize, kCloseButtonSize));
  close_button->SetBorder(nullptr);

  const gfx::Insets margin =
      gfx::Insets().set_left_right(kCloseButtonMargin, kCloseButtonMargin);
  close_button->SetProperty(views::kMarginsKey, margin);

  close_button_ = AddChildView(std::move(close_button));
  SetIsShowingNudge(false);
}

BrowserFrameView* ToolbarGlicButton::GetBrowserFrameView() const {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_window_interface_);
  // 'browser_view' can be null during startup before the BrowserView is added
  // to a widget and is associated to `browser_window_interface_`
  if (!browser_view) {
    return nullptr;
  }

  return browser_view->browser_widget()->GetFrameView();
}

ui::ColorId ToolbarGlicButton::GetBackgroundColor() {
  std::optional<SkColor> background = ToolbarButton::GetBackgroundColor();
  return background.value_or(kColorToolbarButtonBackgroundHighlightedDefault);
}

BEGIN_METADATA(ToolbarGlicButton)
END_METADATA
}  // namespace glic
