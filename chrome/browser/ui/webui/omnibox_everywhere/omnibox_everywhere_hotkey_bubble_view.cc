// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_hotkey_bubble_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace omnibox_everywhere {

namespace {

// Bubble metrics.
constexpr int kBubbleWidth = 219;
constexpr int kBubbleCornerRadius = 8;
constexpr int kBubbleMargin = 8;
constexpr int kContentsVerticalSpacing = 4;

// Header label metrics.
constexpr auto kHeaderBorderInsets = gfx::Insets::TLBR(2, 8, 2, 8);

// Preset option row metrics.
constexpr int kOptionButtonHeight = 36;
constexpr int kOptionButtonWidth = kBubbleWidth - (2 * kBubbleMargin);
constexpr int kOptionHoverCornerRadius = 8;
constexpr auto kOptionLayoutInsets = gfx::Insets::VH(4, 4);
constexpr int kOptionBadgeSpacing = 4;

// Shortcut token badge metrics.
constexpr int kBadgeCornerRadius = 6;
constexpr auto kBadgeBorderInsets = gfx::Insets::TLBR(4, 8, 4, 8);

struct HotkeyBubbleController {
  std::unique_ptr<OmniboxEverywhereHotkeyBubbleView> delegate;
  std::unique_ptr<views::Widget> widget;
};

HotkeyBubbleController& GetHotkeyBubbleController() {
  static base::NoDestructor<HotkeyBubbleController> controller;
  return *controller;
}

// A custom button representing a single preset shortcut row with rounded
// badges.
class PresetOptionButton : public views::Button {
  METADATA_HEADER(PresetOptionButton, views::Button)

 public:
  PresetOptionButton(PressedCallback callback,
                     const std::vector<std::string>& tokens)
      : views::Button(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetInstallFocusRingOnFocus(false);
    SetNotifyEnterExitOnChild(true);

    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, kOptionLayoutInsets,
        kOptionBadgeSpacing));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    std::u16string accessible_name;
    for (size_t i = 0; i < tokens.size(); ++i) {
      if (i > 0) {
        accessible_name += u" + ";
      }
      accessible_name += base::UTF8ToUTF16(tokens[i]);

      auto badge = std::make_unique<views::Label>(
          base::UTF8ToUTF16(tokens[i]), views::style::CONTEXT_LABEL,
          views::style::STYLE_BODY_3_MEDIUM);
      badge->SetCanProcessEventsWithinSubtree(false);
      badge->SetAutoColorReadabilityEnabled(false);
      badge->SetEnabledColor(ui::kColorSysOnPrimaryContainer);
      badge->SetBackground(views::CreateRoundedRectBackground(
          ui::kColorSysPrimaryContainer, kBadgeCornerRadius));
      badge->SetBorder(views::CreateEmptyBorder(kBadgeBorderInsets));
      AddChildView(std::move(badge));
    }

    GetViewAccessibility().SetRole(ax::mojom::Role::kListBoxOption);
    GetViewAccessibility().SetName(accessible_name);
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    gfx::Size preferred = views::Button::CalculatePreferredSize(available_size);
    return gfx::Size(std::max(kOptionButtonWidth, preferred.width()),
                     kOptionButtonHeight);
  }

  ui::Cursor GetCursor(const ui::MouseEvent& event) override {
    return ui::mojom::CursorType::kHand;
  }

  KeyClickAction GetKeyClickActionForEvent(const ui::KeyEvent& event) override {
    if (event.key_code() == ui::VKEY_RETURN) {
      return KeyClickAction::kOnKeyPress;
    }
    return views::Button::GetKeyClickActionForEvent(event);
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    UpdateBackground();
  }

  void OnFocus() override {
    views::Button::OnFocus();
    UpdateBackground();
  }

  void OnBlur() override {
    views::Button::OnBlur();
    UpdateBackground();
  }

  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    UpdateBackground();
  }

 private:
  void UpdateBackground() {
    if (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED ||
        HasFocus()) {
      SetBackground(views::CreateRoundedRectBackground(
          ui::kColorSysStateHoverOnSubtle, kOptionHoverCornerRadius));
    } else {
      SetBackground(nullptr);
    }
    SchedulePaint();
  }
};

BEGIN_METADATA(PresetOptionButton)
END_METADATA

}  // namespace

// static
void OnBubbleClosed(OmniboxEverywhereHotkeyBubbleView* bubble,
                    views::Widget::ClosedReason reason) {
  auto& controller = GetHotkeyBubbleController();
  if (controller.delegate.get() == bubble) {
    base::OnceClosure on_closed = bubble->take_on_closed();
    if (on_closed) {
      std::move(on_closed).Run();
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, controller.widget.release());
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, controller.delegate.release());
  }
}

// static
void OmniboxEverywhereHotkeyBubbleView::Show(
    views::Widget* parent_widget,
    const gfx::Rect& anchor_rect,
    SelectHotkeyCallback on_select_hotkey,
    base::OnceClosure on_closed) {
  CHECK(parent_widget);
  CloseIfOpen();

  auto& controller = GetHotkeyBubbleController();
  controller.delegate = std::make_unique<OmniboxEverywhereHotkeyBubbleView>(
      parent_widget, anchor_rect, std::move(on_select_hotkey),
      std::move(on_closed));
  OmniboxEverywhereHotkeyBubbleView* delegate_ptr = controller.delegate.get();
  controller.widget = views::BubbleDialogDelegate::CreateBubble(
      delegate_ptr,
      base::BindOnce(&OnBubbleClosed, base::Unretained(delegate_ptr)));
  controller.widget->Show();
}

// static
void OmniboxEverywhereHotkeyBubbleView::CloseIfOpen() {
  auto& controller = GetHotkeyBubbleController();
  if (controller.widget && !controller.widget->IsClosed()) {
    controller.widget->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
  }
}

// static
views::Widget* OmniboxEverywhereHotkeyBubbleView::GetWidgetForTesting() {
  return GetHotkeyBubbleController().widget.get();
}

// static
OmniboxEverywhereHotkeyBubbleView*
OmniboxEverywhereHotkeyBubbleView::GetCurrentForTesting() {
  return GetHotkeyBubbleController().delegate.get();
}

OmniboxEverywhereHotkeyBubbleView::OmniboxEverywhereHotkeyBubbleView(
    views::Widget* parent_widget,
    const gfx::Rect& anchor_rect,
    SelectHotkeyCallback on_select_hotkey,
    base::OnceClosure on_closed)
    : views::BubbleDialogDelegate(nullptr,
                                  views::BubbleBorder::NONE,
                                  views::BubbleBorder::DIALOG_SHADOW,
                                  /*autosize=*/true),
      parent_widget_(parent_widget),
      on_select_hotkey_(std::move(on_select_hotkey)),
      on_closed_(std::move(on_closed)) {
  CHECK(parent_widget_);
  parent_widget_observation_.Observe(parent_widget_);
  set_parent_window(parent_widget_->GetNativeView());
  SetAnchorRect(anchor_rect);
  SetArrow(views::BubbleBorder::NONE);
  set_shadow(views::BubbleBorder::DIALOG_SHADOW);
  SetBackgroundColor(ui::kColorSysSurface);
  set_corner_radius(kBubbleCornerRadius);
  set_margins(gfx::Insets(kBubbleMargin));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(false);
  SetShowTitle(false);
  set_close_on_deactivate(true);
  SetEnableArrowKeyTraversal(true);

  auto contents_view = std::make_unique<views::View>();
  auto* layout =
      contents_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          kContentsVerticalSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  // Header label
  auto header_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_LOOMNIBOX_FRE_SELECT_KEYBOARD_SHORTCUT),
      views::style::CONTEXT_LABEL, views::style::STYLE_BODY_5_MEDIUM);
  header_label->SetAutoColorReadabilityEnabled(false);
  header_label->SetEnabledColor(ui::kColorSysOnSurfaceSubtle);
  header_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  header_label->SetBorder(views::CreateEmptyBorder(kHeaderBorderInsets));
  contents_view->AddChildView(std::move(header_label));

  // Preset option buttons
  std::vector<std::string> presets =
      omnibox_everywhere::prefs::GetAvailableHotkeyPresets();
  for (const auto& preset : presets) {
    ui::Accelerator accelerator = ui::Command::StringToAccelerator(preset);
    std::vector<std::string> tokens =
        omnibox_everywhere::prefs::GetOmniboxEverywhereHotkeyTokens(
            accelerator);
    auto button = std::make_unique<PresetOptionButton>(
        base::BindRepeating(
            [](OmniboxEverywhereHotkeyBubbleView* bubble,
               const std::string& hotkey) {
              if (bubble->on_select_hotkey_) {
                bubble->on_select_hotkey_.Run(hotkey);
              }
              OmniboxEverywhereHotkeyBubbleView::CloseIfOpen();
              if (bubble->parent_widget_ &&
                  !bubble->parent_widget_->IsClosed()) {
                bubble->parent_widget_->Activate();
              }
            },
            base::Unretained(this), preset),
        tokens);
    contents_view->AddChildView(std::move(button));
  }
  SetContentsView(std::move(contents_view));
}

OmniboxEverywhereHotkeyBubbleView::~OmniboxEverywhereHotkeyBubbleView() =
    default;

gfx::Rect OmniboxEverywhereHotkeyBubbleView::GetBubbleBounds() {
  if (!GetWidget() || !GetWidget()->client_view() || !GetBubbleFrameView() ||
      !GetContentsView()) {
    return views::BubbleDialogDelegate::GetBubbleBounds();
  }
  const gfx::Rect anchor = GetAnchorRect();
  if (anchor.IsEmpty()) {
    return views::BubbleDialogDelegate::GetBubbleBounds();
  }
  const gfx::Size bubble_size =
      views::BubbleDialogDelegate::GetBubbleBounds().size();
  const gfx::Insets insets = GetBubbleFrameView()->GetInsets();
  gfx::Rect bounds(anchor.x() - insets.left(), anchor.y() - insets.top(),
                   bubble_size.width(), bubble_size.height());

  display::Screen* screen = display::Screen::Get();
  if (screen) {
    display::Display display = screen->GetDisplayNearestPoint(anchor.origin());
    bounds.AdjustToFit(display.work_area());
  }
  return bounds;
}

bool OmniboxEverywhereHotkeyBubbleView::Cancel() {
  if (parent_widget_ && !parent_widget_->IsClosed()) {
    parent_widget_->Activate();
  }
  return true;
}

void OmniboxEverywhereHotkeyBubbleView::WindowClosing() {
  if (on_closed_) {
    std::move(on_closed_).Run();
  }
}

void OmniboxEverywhereHotkeyBubbleView::OnWidgetDestroying(
    views::Widget* widget) {
  if (parent_widget_ == widget) {
    parent_widget_observation_.Reset();
    parent_widget_ = nullptr;
  }
}

}  // namespace omnibox_everywhere
