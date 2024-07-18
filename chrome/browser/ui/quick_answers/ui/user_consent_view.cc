// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/user_consent_view.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_view.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/common/content_switches.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/screen.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
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
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace quick_answers {

namespace {

// Main view (or common) specs.
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

views::Builder<views::Label> GetConfiguredLabelBuilder(int font_size_delta) {
  return views::Builder<views::Label>()
      // TODO(b/340628664): This is from old code. Consider if we can remove
      // AutoColorReadabilityEnabled=false.
      .SetAutoColorReadabilityEnabled(false)
      .SetLineHeight(kLineHeightDip)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetFontList(views::Label::GetDefaultFontList().DeriveWithSizeDelta(
          font_size_delta));
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
};

BEGIN_METADATA(CustomizedLabelButton)
END_METADATA

}  // namespace

// UserConsentView
// -------------------------------------------------------------

UserConsentView::UserConsentView(
    const std::u16string& intent_type,
    const std::u16string& intent_text,
    base::WeakPtr<QuickAnswersUiController> controller)
    : chromeos::ReadWriteCardsView(controller->GetReadWriteCardsUiController()),
      controller_(std::move(controller)),
      focus_search_(this,
                    base::BindRepeating(&UserConsentView::GetFocusableViews,
                                        base::Unretained(this))) {
  if (intent_type.empty() || intent_text.empty()) {
    title_text_ = l10n_util::GetStringUTF16(
        IDS_QUICK_ANSWERS_USER_NOTICE_VIEW_TITLE_TEXT);
  } else {
    // TODO(b/340628664): pass intent type enum and stop building a UI string
    // with string concatenation as it can cause complications in UI
    // translations.
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

void UserConsentView::OnFocus() {
  // Unless screen-reader mode is enabled, transfer the focus to an actionable
  // button, otherwise retain to read out its contents.
  if (QuickAnswersState::Get()->spoken_feedback_enabled()) {
    no_thanks_button_->RequestFocus();
  }
}

void UserConsentView::OnThemeChanged() {
  views::View::OnThemeChanged();

  // TODO(b/340628664): Delete `UserConsentView::OnThemeChanged`. Let
  // `views::Label`, etc handle those color changes.
  SetBackground(views::CreateSolidBackground(
      GetColorProvider()->GetColor(ui::kColorPrimaryBackground)));
  title_->SetEnabledColor(
      GetColorProvider()->GetColor(ui::kColorLabelForeground));
  description_->SetEnabledColor(
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

void UserConsentView::UpdateBoundsForQuickAnswers() {
  // TODO(b/331271987): Remove this and the interface.
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

void UserConsentView::InitLayout() {
  SetUseDefaultFillLayout(true);

  // TODO(b/340628664): Use views::Builder.
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
  google_icon->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kGoogleColorIcon, gfx::kPlaceholderColor,
      kGoogleIconSizeDip));

  // Content.
  InitContent();
}

void UserConsentView::InitContent() {
  const gfx::Insets margin = gfx::Insets::TLBR(0, 0, kContentSpacingDip, 0);

  views::Label* title;
  views::Label* description;

  content_ = main_view_->AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetIgnoreDefaultMainAxisMargins(true)
          .SetInteriorMargin(kContentInsets)
          .SetCollapseMargins(true)
          .AddChild(
              GetConfiguredLabelBuilder(kTitleFontSizeDelta)
                  .CopyAddressTo(&title)
                  .SetText(title_text_)
                  .SetProperty(views::kMarginsKey, margin)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::MinimumFlexSizeRule::kScaleToMinimum,
                                   views::MaximumFlexSizeRule::kPreferred)))
          .AddChild(
              GetConfiguredLabelBuilder(kDescFontSizeDelta)
                  .CopyAddressTo(&description)
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_DESC_TEXT))
                  .SetMultiLine(true)
                  .SetProperty(views::kMarginsKey, margin)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::MinimumFlexSizeRule::kScaleToMinimum,
                                   views::MaximumFlexSizeRule::kPreferred,
                                   /*adjust_height_for_width=*/true)))
          .Build());

  CHECK(!title_) << "Title label is already created";
  CHECK(title);
  title_ = title;

  CHECK(!description_) << "Description label is already created";
  CHECK(description);
  description_ = description;

  // Button bar.
  InitButtonBar();
}

void UserConsentView::InitButtonBar() {
  // TODO(b/340628664): Use views::Builder.
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
      ShouldUseCompactButtonLayout(context_menu_bounds().width()));
  // TODO(b/340628664): Consider if we can set min size for UserConsentView
  // itself.
  // Use MinimumFlexSizeRule=kPreferred instead of kScaleToZero, etc to avoid
  // making an un-readable but actionable button. This is to avoid showing
  // following UI:
  //
  // Title
  // Description
  // [] []
  //
  // Two buttons are shown without text because all button texts get truncated
  // for insufficient space.
  no_thanks_button->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));
  no_thanks_button_ = button_bar->AddChildView(std::move(no_thanks_button));

  // Allow button
  auto allow_button = std::make_unique<CustomizedLabelButton>(
      base::BindRepeating(
          [](base::WeakPtr<QuickAnswersUiController> controller) {
            if (controller) {
              // When user consent is accepted, QuickAnswersView will be
              // displayed instead of dismissing the menu.
              controller->GetReadWriteCardsUiController()
                  .pre_target_handler()
                  .set_dismiss_anchor_menu_on_view_closed(false);
              controller->OnUserConsentResult(true);
            }
          },
          controller_),
      l10n_util::GetStringUTF16(
          IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_ALLOW_BUTTON),
      ShouldUseCompactButtonLayout(context_menu_bounds().width()));
  allow_button->SetStyle(ui::ButtonStyle::kProminent);
  // TODO(b/340628664): Consider if we can set min size for UserConsentView
  // itself.
  // Use MinimumFlexSizeRule=kPreferred instead of kScaleToZero, etc to avoid
  // making an un-readable but actionable button.
  allow_button->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));
  allow_button_ = button_bar->AddChildView(std::move(allow_button));

  // Set preferred size of `button_bar` as a minimum x-axis size of `content_`.
  // We intentionally let the layout overflow in x-axis. Without this,
  // `content_` will try to render in the available size and end up in a wrong
  // height.
  CHECK(content_);
  content_->SetMinimumCrossAxisSize(button_bar->GetPreferredSize().width());
}

BEGIN_METADATA(UserConsentView)
END_METADATA

}  // namespace quick_answers
