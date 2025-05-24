// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic_button.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#endif

namespace glic {

GlicButton::GlicButton(TabStripController* tab_strip_controller,
                       PressedCallback pressed_callback,
                       PressedCallback close_pressed_callback,
                       base::RepeatingClosure hovered_callback,
                       base::RepeatingClosure mouse_down_callback,
                       const gfx::VectorIcon& icon,
                       const std::u16string& tooltip)
    : TabStripNudgeButton(tab_strip_controller,
                          std::move(pressed_callback),
                          std::move(close_pressed_callback),
                          tooltip,
                          kGlicNudgeButtonElementId,
                          Edge::kNone,
                          icon),
      menu_model_(CreateMenuModel()),
      tab_strip_controller_(tab_strip_controller),
      hovered_callback_(std::move(hovered_callback)),
      mouse_down_callback_(std::move(mouse_down_callback)) {
  SetProperty(views::kElementIdentifierKey, kGlicButtonElementId);

  set_context_menu_controller(this);

  SetTooltipText(tooltip);
  GetViewAccessibility().SetName(tooltip);

  SetForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);

  SetForegroundFrameInactiveColorId(kColorNewTabButtonForegroundFrameInactive);
  SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorNewTabButtonCRBackgroundFrameInactive);

  UpdateColors();

  SetVisible(true);

  SetFocusBehavior(FocusBehavior::ALWAYS);

  auto* const layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
}

GlicButton::~GlicButton() = default;


void GlicButton::SetIsShowingNudge(bool is_showing) {
  if (is_showing) {
    SetCloseButtonFocusBehavior(FocusBehavior::ALWAYS);
    AnnounceNudgeShown();
  } else {
    SetCloseButtonFocusBehavior(FocusBehavior::NEVER);
  }
  is_showing_nudge_ = is_showing;
  PreferredSizeChanged();
}

gfx::Size GlicButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int full_width =
      GetLayoutManager()->GetPreferredSize(this, available_size).width();

  const int height = TabStripControlButton::CalculatePreferredSize(
                         views::SizeBounds(full_width, available_size.height()))
                         .height();
  // Set collapsed size to a square.
  const int collapsed_width = height;
  const int width = std::lerp(collapsed_width, full_width, GetWidthFactor());

  return gfx::Size(width, height);
}

void GlicButton::StateChanged(ButtonState old_state) {
  TabStripNudgeButton::StateChanged(old_state);
  if (old_state == STATE_NORMAL && GetState() == STATE_HOVERED &&
      hovered_callback_) {
    hovered_callback_.Run();
  }
}

void GlicButton::SetDropToAttachIndicator(bool indicate) {
  if (indicate) {
    SetBackgroundFrameActiveColorId(ui::kColorSysStateHeaderHover);
  } else {
    SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  }
}

gfx::Rect GlicButton::GetBoundsWithInset() const {
  gfx::Rect bounds = GetBoundsInScreen();
  bounds.Inset(GetInsets());
  return bounds;
}

void GlicButton::ShowContextMenuForViewImpl(
    View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (!profile_prefs()->GetBoolean(glic::prefs::kGlicPinnedToTabstrip)) {
    return;
  }

  menu_anchor_higlight_ = AddAnchorHighlight();

  menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
      menu_model_.get(),
      base::BindRepeating(&GlicButton::OnMenuClosed, base::Unretained(this)));
  menu_model_adapter_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                                   ui::EF_RIGHT_MOUSE_BUTTON);
  std::unique_ptr<views::MenuItemView> root = menu_model_adapter_->CreateMenu();
  menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(root),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void GlicButton::ExecuteCommand(int command_id, int event_flags) {
  CHECK(command_id == IDC_GLIC_TOGGLE_PIN);
  profile_prefs()->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);
}

bool GlicButton::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && mouse_down_callback_) {
    mouse_down_callback_.Run();
    return true;
  }
  return false;
}

bool GlicButton::IsContextMenuShowingForTest() {
  return menu_runner_ && menu_runner_->IsRunning();
}

std::unique_ptr<ui::SimpleMenuModel> GlicButton::CreateMenuModel() {
  std::unique_ptr<ui::SimpleMenuModel> model =
      std::make_unique<ui::SimpleMenuModel>(this);
  model->AddItemWithStringIdAndIcon(
      IDC_GLIC_TOGGLE_PIN, IDS_GLIC_BUTTON_CXMENU_UNPIN,
      ui::ImageModel::FromVectorIcon(kKeepOffIcon, ui::kColorIcon, 16));
  return model;
}

void GlicButton::OnMenuClosed() {
  menu_anchor_higlight_.reset();
  menu_runner_.reset();
}

void GlicButton::AnnounceNudgeShown() {
#if BUILDFLAG(ENABLE_GLIC)
  auto announcement = l10n_util::GetStringFUTF16(
      IDS_GLIC_CONTEXTUAL_CUEING_ANNOUNCEMENT,
      GlicLauncherConfiguration::GetGlobalHotkey().GetShortcutText());
  GetViewAccessibility().AnnounceAlert(announcement);
#else
  NOTIMPLEMENTED();
#endif
}

BEGIN_METADATA(GlicButton)
END_METADATA

}  // namespace glic
