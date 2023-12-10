// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/user_consent_view.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/common/content_switches.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/screen.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace quick_answers {

namespace {

// Main view (or common) specs.
constexpr int kMarginDip = 10;
constexpr int kLineHeightDip = 20;
constexpr int kContentSpacingDip = 8;
constexpr auto kMainViewInsets = gfx::Insets::TLBR(16, 12, 16, 16);
constexpr auto kContentInsets = gfx::Insets::TLBR(0, 12, 0, 0);

// Google icon.
constexpr int kGoogleIconSizeDip = 16;

// Text font size delta.
constexpr int kTitleFontSizeDelta = 2;
constexpr int kDescFontSizeDelta = 1;

// Buttons common.
constexpr int kButtonSpacingDip = 8;
constexpr auto kButtonBarInsets = gfx::Insets::TLBR(8, 0, 0, 0);
constexpr auto kButtonInsets = gfx::Insets::TLBR(6, 16, 6, 16);
constexpr int kButtonFontSizeDelta = 1;

// Compact buttons layout.
constexpr int kCompactButtonLayoutThreshold = 200;
constexpr auto kCompactButtonInsets = gfx::Insets::TLBR(6, 12, 6, 12);
constexpr int kCompactButtonFontSizeDelta = 0;

int GetActualLabelWidth(int anchor_view_width) {
  return anchor_view_width - kMainViewInsets.width() - kContentInsets.width() -
         kGoogleIconSizeDip;
}

bool ShouldUseCompactButtonLayout(int anchor_view_width) {
  return GetActualLabelWidth(anchor_view_width) < kCompactButtonLayoutThreshold;
}

// Create and return a simple label with provided specs.
std::unique_ptr<views::Label> CreateLabel(const std::u16string& text,
                                          int font_size_delta) {
  auto label = std::make_unique<views::Label>(text);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetLineHeight(kLineHeightDip);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetFontList(
      views::Label::GetDefaultFontList().DeriveWithSizeDelta(font_size_delta));
  return label;
}

// views::LabelButton with custom line-height, color and font-list for the
// underlying label.
class CustomizedLabelButton : public views::MdTextButton {
  METADATA_HEADER(CustomizedLabelButton, views::MdTextButton)

 public:
  CustomizedLabelButton(PressedCallback callback,
                        const std::u16string& text,
                        bool is_compact)
      : MdTextButton(std::move(callback), text) {
    SetCustomPadding(is_compact ? kCompactButtonInsets : kButtonInsets);
    label()->SetLineHeight(kLineHeightDip);
    label()->SetFontList(
        views::Label::GetDefaultFontList()
            .DeriveWithSizeDelta(is_compact ? kCompactButtonFontSizeDelta
                                            : kButtonFontSizeDelta)
            .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  }

  // Disallow copy and assign.
  CustomizedLabelButton(const CustomizedLabelButton&) = delete;
  CustomizedLabelButton& operator=(const CustomizedLabelButton&) = delete;

  ~CustomizedLabelButton() override = default;

  // views::View:
  const char* GetClassName() const override { return "CustomizedLabelButton"; }
};

BEGIN_METADATA(CustomizedLabelButton)
END_METADATA

}  // namespace

// UserConsentView
// -------------------------------------------------------------

UserConsentView::UserConsentView(
    const gfx::Rect& anchor_view_bounds,
    const std::u16string& intent_type,
    const std::u16string& intent_text,
    base::WeakPtr<QuickAnswersUiController> controller)
    : anchor_view_bounds_(anchor_view_bounds),
      event_handler_(this),
      controller_(std::move(controller)),
      focus_search_(this,
                    base::BindRepeating(&UserConsentView::GetFocusableViews,
                                        base::Unretained(this))) {
  if (intent_type.empty() || intent_text.empty()) {
    title_text_ = l10n_util::GetStringUTF16(
        IDS_QUICK_ANSWERS_USER_NOTICE_VIEW_TITLE_TEXT);
  } else {
    title_text_ = l10n_util::GetStringFUTF16(
        IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_TITLE_TEXT_WITH_INTENT, intent_type,
        intent_text);
  }

  InitLayout();

  // Focus should cycle to each of the buttons the view contains and back to it.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();
  views::FocusRing::Install(this);

  // Read out user-consent text if screen-reader is active.
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      IDS_QUICK_ANSWERS_USER_NOTICE_VIEW_A11Y_INFO_ALERT_TEXT));
}

UserConsentView::~UserConsentView() = default;

views::UniqueWidgetPtr UserConsentView::CreateWidget(
    const gfx::Rect& anchor_view_bounds,
    const std::u16string& intent_type,
    const std::u16string& intent_text,
    base::WeakPtr<QuickAnswersUiController> controller) {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;

  // Parent the widget to the owner of the menu.
  auto* active_menu_controller = views::MenuController::GetActiveInstance();
  DCHECK(active_menu_controller && active_menu_controller->owner());

  // This widget has to be a child of menu owner's widget to make keyboard focus
  // work.
  params.parent = active_menu_controller->owner()->GetNativeView();
  params.child = true;
  params.name = kWidgetName;

  views::UniqueWidgetPtr widget =
      std::make_unique<views::Widget>(std::move(params));
  UserConsentView* user_consent_view =
      widget->SetContentsView(std::make_unique<UserConsentView>(
          anchor_view_bounds, intent_type, intent_text, controller));
  user_consent_view->UpdateWidgetBounds();

  // Allow tooltips to be shown despite menu-controller owning capture.
  widget->SetNativeWindowProperty(
      views::TooltipManager::kGroupingPropertyKey,
      reinterpret_cast<void*>(views::MenuConfig::kMenuControllerGroupingId));

  return widget;
}

gfx::Size UserConsentView::CalculatePreferredSize() const {
  // View should match width of the anchor.
  auto width = anchor_view_bounds_.width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void UserConsentView::OnFocus() {
  // Unless screen-reader mode is enabled, transfer the focus to an actionable
  // button, otherwise retain to read out its contents.
  if (QuickAnswersState::Get()->spoken_feedback_enabled()) {
    no_thanks_button_->RequestFocus();
  }
}

void UserConsentView::OnThemeChanged() {
  views::View::OnThemeChanged();

  SetBackground(views::CreateSolidBackground(
      GetColorProvider()->GetColor(ui::kColorPrimaryBackground)));
  title_->SetEnabledColor(
      GetColorProvider()->GetColor(ui::kColorLabelForeground));
  desc_->SetEnabledColor(
      GetColorProvider()->GetColor(ui::kColorLabelForegroundSecondary));
}

views::FocusTraversable* UserConsentView::GetPaneFocusTraversable() {
  return &focus_search_;
}

void UserConsentView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(title_text_);
  auto desc_text = l10n_util::GetStringFUTF8(
      IDS_QUICK_ANSWERS_USER_NOTICE_VIEW_A11Y_INFO_DESC_TEMPLATE,
      l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_DESC_TEXT));
  node_data->SetDescription(desc_text);
}

std::vector<views::View*> UserConsentView::GetFocusableViews() {
  std::vector<views::View*> focusable_views;
  // The view itself is not included in focus loop, unless screen-reader is on.
  if (QuickAnswersState::Get()->spoken_feedback_enabled()) {
    focusable_views.push_back(this);
  }
  focusable_views.push_back(no_thanks_button_);
  focusable_views.push_back(allow_button_);
  return focusable_views;
}

void UserConsentView::UpdateAnchorViewBounds(
    const gfx::Rect& anchor_view_bounds) {
  anchor_view_bounds_ = anchor_view_bounds;
  UpdateWidgetBounds();
}

void UserConsentView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Main-view Layout.
  main_view_ = AddChildView(std::make_unique<views::View>());
  auto* layout =
      main_view_->SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetInteriorMargin(kMainViewInsets)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  // Google icon.
  auto* google_icon =
      main_view_->AddChildView(std::make_unique<views::ImageView>());
  google_icon->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR((kLineHeightDip - kGoogleIconSizeDip) / 2, 0, 0, 0)));
  google_icon->SetImage(gfx::CreateVectorIcon(vector_icons::kGoogleColorIcon,
                                              kGoogleIconSizeDip,
                                              gfx::kPlaceholderColor));

  // Content.
  InitContent();
}

void UserConsentView::InitContent() {
  // Layout.
  content_ = main_view_->AddChildView(std::make_unique<views::View>());

  auto* layout =
      content_->SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetInteriorMargin(kContentInsets)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, 0, kContentSpacingDip, 0));

  // Title.
  title_ =
      content_->AddChildView(CreateLabel(title_text_, kTitleFontSizeDelta));
  // Set the maximum width of the label to the width it would need to be for the
  // UserConsentView to be the same width as the anchor, so its preferred size
  // will be calculated correctly.
  int maximum_width = GetActualLabelWidth(anchor_view_bounds_.width());
  title_->SetMaximumWidthSingleLine(maximum_width);

  // Description.
  desc_ = content_->AddChildView(CreateLabel(
      l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_DESC_TEXT),
      kDescFontSizeDelta));
  desc_->SetMultiLine(true);

  desc_->SetMaximumWidth(maximum_width);

  // Button bar.
  InitButtonBar();
}

void UserConsentView::InitButtonBar() {
  // Layout.
  auto* button_bar = content_->AddChildView(std::make_unique<views::View>());
  auto* layout =
      button_bar->SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetInteriorMargin(kButtonBarInsets)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, 0, 0, kButtonSpacingDip));

  // No thanks button.
  auto no_thanks_button = std::make_unique<CustomizedLabelButton>(
      base::BindRepeating(&QuickAnswersUiController::OnUserConsentResult,
                          controller_, false),
      l10n_util::GetStringUTF16(
          IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_NO_THANKS_BUTTON),
      ShouldUseCompactButtonLayout(anchor_view_bounds_.width()));
  no_thanks_button_ = button_bar->AddChildView(std::move(no_thanks_button));

  // Allow button
  auto allow_button = std::make_unique<CustomizedLabelButton>(
      base::BindRepeating(
          [](chromeos::editor_menu::PreTargetHandler* handler,
             base::WeakPtr<QuickAnswersUiController> controller) {
            // When user consent is accepted, QuickAnswersView will be
            // displayed instead of dismissing the menu.
            handler->set_dismiss_anchor_menu_on_view_closed(false);
            if (controller) {
              controller->OnUserConsentResult(true);
            }
          },
          &event_handler_, controller_),
      l10n_util::GetStringUTF16(
          IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_ALLOW_BUTTON),
      ShouldUseCompactButtonLayout(anchor_view_bounds_.width()));
  allow_button->SetProminent(true);
  allow_button_ = button_bar->AddChildView(std::move(allow_button));
}

void UserConsentView::UpdateWidgetBounds() {
  const gfx::Size size = GetPreferredSize();
  int x = anchor_view_bounds_.x();
  int y = anchor_view_bounds_.y() - size.height() - kMarginDip;
  if (y < display::Screen::GetScreen()
              ->GetDisplayMatching(anchor_view_bounds_)
              .work_area()
              .y()) {
    y = anchor_view_bounds_.bottom() + kMarginDip;
  }
  gfx::Rect bounds({x, y}, size);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // For Ash, convert the position relative to the screen.
  // For Lacros, `bounds` is already relative to the toplevel window and the
  // position will be calculated on server side.
  wm::ConvertRectFromScreen(GetWidget()->GetNativeWindow()->parent(), &bounds);
#endif

  GetWidget()->SetBounds(bounds);
}

BEGIN_METADATA(UserConsentView, views::View)
END_METADATA

}  // namespace quick_answers
