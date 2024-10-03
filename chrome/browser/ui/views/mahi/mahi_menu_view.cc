// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_constants.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler_view.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_constants.h"
#include "chromeos/components/magic_boost/public/cpp/views/experiment_badge.h"
#include "chromeos/components/mahi/public/cpp/mahi_browser_util.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_content_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace chromeos::mahi {

namespace {

constexpr char kWidgetName[] = "MahiMenuViewWidget";

constexpr gfx::Insets kMenuPadding = gfx::Insets::TLBR(12, 16, 12, 14);
constexpr int kButtonHeight = 16;
constexpr int kButtonCornerRadius = 8;
constexpr gfx::Insets kButtonPadding = gfx::Insets::VH(6, 8);
constexpr gfx::Insets kHeaderRowPadding = gfx::Insets::TLBR(0, 0, 12, 0);
constexpr int kHeaderRowSpacing = 8;
constexpr int kButtonsRowSpacing = 12;
constexpr int kButtonTextfieldSpacing = 16;
constexpr int kButtonImageLabelSpacing = 4;
constexpr int kButtonBorderThickness = 1;
constexpr int kTextfieldContainerSpacing = 8;
constexpr int kInputContainerCornerRadius = 8;
constexpr gfx::Insets kTextfieldButtonPadding = gfx::Insets::VH(0, 8);

void StyleMenuButton(views::LabelButton* button, const gfx::VectorIcon& icon) {
  button->SetLabelStyle(views::style::STYLE_BODY_4_EMPHASIS);
  button->SetImageModel(views::Button::ButtonState::STATE_NORMAL,
                        ui::ImageModel::FromVectorIcon(
                            icon, ui::kColorSysOnSurface, kButtonHeight));
  button->SetTextColorId(views::LabelButton::ButtonState::STATE_NORMAL,
                         ui::kColorSysOnSurface);
  button->SetImageLabelSpacing(kButtonImageLabelSpacing);
  button->SetBorder(views::CreatePaddedBorder(
      views::CreateThemedRoundedRectBorder(kButtonBorderThickness,
                                           kButtonCornerRadius,
                                           ui::kColorSysTonalOutline),
      kButtonPadding));
}

// Custom widget to ensure the MahiMenuView follows the same theme as the
// browser context menu.
class MahiMenuWidget : public views::Widget {
 public:
  explicit MahiMenuWidget(views::Widget::InitParams init_params)
      : views::Widget(std::move(init_params)) {}
  MahiMenuWidget(const Widget&) = delete;
  MahiMenuWidget& operator=(const Widget&) = delete;
  ~MahiMenuWidget() override = default;

 protected:
  const ui::ColorProvider* GetColorProvider() const override {
    // Get the color provider for the active menu controller's owner if possible
    // to match the color theme for the browser.
    auto* active_menu_controller = views::MenuController::GetActiveInstance();

    // The menu might already be closed.
    if (active_menu_controller && active_menu_controller->owner()) {
      return active_menu_controller->owner()->GetColorProvider();
    }

    return views::Widget::GetColorProvider();
  }
};

}  // namespace

// Controller for the `textfield_` owned by `MahiMenuView`. Enables the
// `submit_question_button` only when the `textfield_` contains some input.
// Also, submits a question if the user presses the enter key while focused on
// the textfield.
class MahiMenuView::MenuTextfieldController
    : public views::TextfieldController {
 public:
  explicit MenuTextfieldController(base::WeakPtr<MahiMenuView> menu_view)
      : menu_view_(menu_view) {}
  MenuTextfieldController(const MenuTextfieldController&) = delete;
  MenuTextfieldController& operator=(const MenuTextfieldController&) = delete;
  ~MenuTextfieldController() override = default;

 private:
  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& event) override {
    // Do not try to send a reply if no text has been input.
    if (!menu_view_ || sender->GetText().empty()) {
      return false;
    }

    if (event.type() == ui::EventType::kKeyPressed &&
        event.key_code() == ui::VKEY_RETURN) {
      menu_view_->OnQuestionSubmitted();
      return true;
    }

    return false;
  }
  void OnAfterUserAction(views::Textfield* sender) override {
    if (!menu_view_) {
      return;
    }

    bool enabled = !sender->GetText().empty();
    menu_view_->GetViewByID(ViewID::kSubmitQuestionButton)->SetEnabled(enabled);
  }

  base::WeakPtr<MahiMenuView> menu_view_;
};

MahiMenuView::MahiMenuView(Surface surface)
    : chromeos::editor_menu::PreTargetHandlerView(
          chromeos::editor_menu::CardType::kMahiDefaultMenu),
      surface_(surface) {
  SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorPrimaryBackground,
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kMenuRadius)));

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetInteriorMargin(kMenuPadding);

  auto header_row = std::make_unique<views::FlexLayoutView>();
  header_row->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_row->SetInteriorMargin(kHeaderRowPadding);

  auto header_left_container = std::make_unique<views::FlexLayoutView>();
  header_left_container->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_left_container->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  header_left_container->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  header_left_container->SetDefault(
      views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, kHeaderRowSpacing));
  header_left_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded)));

  auto* header_label =
      header_left_container->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_ASH_MAHI_MENU_TITLE),
          views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_HEADLINE_5));
  header_label->SetEnabledColorId(ui::kColorSysOnSurface);
  header_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  header_label->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);

  header_left_container->AddChildView(
      std::make_unique<chromeos::ExperimentBadge>());

  header_row->AddChildView(std::move(header_left_container));

  settings_button_ =
      header_row->AddChildView(views::ImageButton::CreateIconButton(
          base::BindRepeating(&MahiMenuView::OnButtonPressed,
                              weak_ptr_factory_.GetWeakPtr(),
                              ::chromeos::mahi::ButtonType::kSettings),
          vector_icons::kSettingsOutlineIcon,
          l10n_util::GetStringUTF16(IDS_EDITOR_MENU_SETTINGS_TOOLTIP)));
  settings_button_->SetID(ViewID::kSettingsButton);

  AddChildView(std::move(header_row));

  // Create row containing the `summary_button_` and `outline_button_`.
  AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetProperty(views::kCrossAxisAlignmentKey,
                       views::LayoutAlignment::kStart)
          .AddChildren(
              views::Builder<views::LabelButton>()
                  .SetID(ViewID::kSummaryButton)
                  .CopyAddressTo(&summary_button_)
                  .SetCallback(base::BindRepeating(
                      &MahiMenuView::OnButtonPressed,
                      weak_ptr_factory_.GetWeakPtr(),
                      ::chromeos::mahi::ButtonType::kSummary))
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_MAHI_SUMMARIZE_BUTTON_LABEL_TEXT))
                  .SetProperty(views::kMarginsKey,
                               gfx::Insets::TLBR(0, 0, 0, kButtonsRowSpacing)),
              views::Builder<views::LabelButton>()
                  .SetID(ViewID::kOutlineButton)
                  .CopyAddressTo(&outline_button_)
                  .SetCallback(base::BindRepeating(
                      &MahiMenuView::OnButtonPressed,
                      weak_ptr_factory_.GetWeakPtr(),
                      ::chromeos::mahi::ButtonType::kOutline))
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_MAHI_OUTLINE_BUTTON_LABEL_TEXT))
                  // TODO(b/330643995): Unhide the outline button once outlines
                  // are ready to be shown by default.
                  .SetVisible(false))
          .Build());

  StyleMenuButton(summary_button_, chromeos::kMahiSummarizeIcon);
  StyleMenuButton(outline_button_, chromeos::kMahiOutlinesIcon);

  textfield_controller_ =
      std::make_unique<MenuTextfieldController>(weak_ptr_factory_.GetWeakPtr());
  AddChildView(CreateInputContainer());

  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_MAHI_MENU_TITLE));
}

MahiMenuView::~MahiMenuView() {
  // `textfield_` keeps a raw pointer to `textfield_controller_` - reset that
  // before destroying the controller.
  textfield_->SetController(nullptr);
}

// static
views::UniqueWidgetPtr MahiMenuView::CreateWidget(
    const gfx::Rect& anchor_view_bounds,
    Surface surface) {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.name = GetWidgetName();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  params.init_properties_container.SetProperty(kIsMahiMenuKey, true);
#endif

  views::UniqueWidgetPtr widget =
      std::make_unique<MahiMenuWidget>(std::move(params));
  MahiMenuView* mahi_menu_view =
      widget->SetContentsView(std::make_unique<MahiMenuView>(surface));
  mahi_menu_view->UpdateBounds(anchor_view_bounds);

  return widget;
}

// static
const char* MahiMenuView::GetWidgetName() {
  return kWidgetName;
}

void MahiMenuView::RequestFocus() {
  views::View::RequestFocus();

  // TODO(b/319735347): Add browsertest for this behavior.
  settings_button_->RequestFocus();
}

void MahiMenuView::UpdateBounds(const gfx::Rect& anchor_view_bounds) {
  // TODO(b/318733414): Move `editor_menu::GetEditorMenuBounds` to a common
  // place for use
  GetWidget()->SetBounds(editor_menu::GetEditorMenuBounds(
      anchor_view_bounds, this, editor_menu::CardType::kMahiDefaultMenu));
}

void MahiMenuView::OnButtonPressed(::chromeos::mahi::ButtonType button_type) {
  auto display = display::Screen::GetScreen()->GetDisplayNearestWindow(
      GetWidget()->GetNativeWindow());
  if (surface_ == Surface::kBrowser) {
    chromeos::MahiWebContentsManager::Get()->OnContextMenuClicked(
        display.id(), button_type,
        /*question=*/std::u16string(), GetBoundsInScreen());
  } else if (surface_ == Surface::kMediaApp) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Only ash chrome has `surface_` = kMediaApp
    CHECK(chromeos::MahiMediaAppContentManager::Get());
    chromeos::MahiMediaAppContentManager::Get()->OnMahiContextMenuClicked(
        display.id(), button_type,
        /*question=*/std::u16string(), GetBoundsInScreen());
#endif
  }

  MahiMenuButton histogram_button_type;
  switch (button_type) {
    case ::chromeos::mahi::ButtonType::kSummary:
      histogram_button_type = MahiMenuButton::kSummaryButton;
      break;
    case ::chromeos::mahi::ButtonType::kOutline:
      // TODO(b/330643995): Remove CHECK_IS_TEST when outlines are
      // ready.
      CHECK_IS_TEST();
      histogram_button_type = MahiMenuButton::kOutlineButton;
      break;
    case ::chromeos::mahi::ButtonType::kSettings:
      histogram_button_type = MahiMenuButton::kSettingsButton;
      break;
    default:
      // This function only handles clicks of type 'kSummary',
      // 'kOutline' and `kSettings`. Other click types are not passed
      // here.
      NOTREACHED();
  }
  base::UmaHistogramEnumeration(kMahiContextMenuButtonClickHistogram,
                                histogram_button_type);
}

void MahiMenuView::OnQuestionSubmitted() {
  auto display = display::Screen::GetScreen()->GetDisplayNearestWindow(
      GetWidget()->GetNativeWindow());
  if (surface_ == Surface::kBrowser) {
    chromeos::MahiWebContentsManager::Get()->OnContextMenuClicked(
        display.id(), /*button_type=*/::chromeos::mahi::ButtonType::kQA,
        textfield_->GetText(), GetBoundsInScreen());
  } else if (surface_ == Surface::kMediaApp) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Only ash chrome has `surface_` = kMediaApp
    CHECK(chromeos::MahiMediaAppContentManager::Get());
    chromeos::MahiMediaAppContentManager::Get()->OnMahiContextMenuClicked(
        display.id(), ::chromeos::mahi::ButtonType::kQA, textfield_->GetText(),
        GetBoundsInScreen());
#endif
  }

  base::UmaHistogramEnumeration(kMahiContextMenuButtonClickHistogram,
                                MahiMenuButton::kSubmitQuestionButton);
}

std::unique_ptr<views::FlexLayoutView> MahiMenuView::CreateInputContainer() {
  auto input_container =
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetBackground(views::CreateThemedRoundedRectBackground(
              ui::kColorSysStateHoverOnSubtle, kInputContainerCornerRadius))
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          .SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(kButtonTextfieldSpacing, 0, 0, 0))
          .AddChildren(
              views::Builder<views::Textfield>()
                  .SetID(ViewID::kTextfield)
                  .CopyAddressTo(&textfield_)
                  .SetController(textfield_controller_.get())
                  .SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT)
                  .SetPlaceholderText(
                      l10n_util::GetStringUTF16(IDS_MAHI_MENU_INPUT_TEXTHOLDER))
                  .SetAccessibleName(
                      l10n_util::GetStringUTF16(IDS_MAHI_MENU_INPUT_TEXTHOLDER))
                  .SetProperty(
                      views::kFlexBehaviorKey,
                      views::FlexSpecification(views::FlexSpecification(
                          views::LayoutOrientation::kHorizontal,
                          views::MinimumFlexSizeRule::kPreferred,
                          views::MaximumFlexSizeRule::kUnbounded)))
                  .SetProperty(views::kMarginsKey,
                               gfx::Insets::TLBR(0, kTextfieldContainerSpacing,
                                                 0, kTextfieldContainerSpacing))
                  .SetBackgroundEnabled(false)
                  .SetBorder(nullptr),
              views::Builder<views::ImageButton>()
                  .SetID(ViewID::kSubmitQuestionButton)
                  .CopyAddressTo(&submit_question_button_)
                  .SetCallback(
                      base::BindRepeating(&MahiMenuView::OnQuestionSubmitted,
                                          weak_ptr_factory_.GetWeakPtr()))
                  .SetImageModel(
                      views::Button::STATE_NORMAL,
                      ui::ImageModel::FromVectorIcon(vector_icons::kSendIcon))
                  .SetImageModel(
                      views::Button::STATE_DISABLED,
                      ui::ImageModel::FromVectorIcon(
                          vector_icons::kSendIcon, ui::kColorSysStateDisabled))
                  .SetAccessibleName(l10n_util::GetStringUTF16(
                      IDS_MAHI_MENU_INPUT_SEND_BUTTON_ACCESSIBLE_NAME))
                  .SetProperty(views::kMarginsKey, kTextfieldButtonPadding)
                  .SetEnabled(false))
          .Build();

  // Focus ring insets need to be negative because we want the focus rings to
  // exceed the textfield bounds horizontally to cover the entire `container`.
  int focus_ring_left_inset = -1 * (kTextfieldContainerSpacing);
  int focus_ring_right_inset =
      -1 * (kTextfieldContainerSpacing + kTextfieldButtonPadding.width() +
            submit_question_button_->GetPreferredSize().width());

  views::FocusRing::Install(textfield_);
  views::FocusRing::Get(textfield_)->SetColorId(ui::kColorSysStateFocusRing);
  views::InstallRoundRectHighlightPathGenerator(
      textfield_,
      gfx::Insets::TLBR(0, focus_ring_left_inset, 0, focus_ring_right_inset),
      kInputContainerCornerRadius);

  return input_container;
}

BEGIN_METADATA(MahiMenuView)
END_METADATA

}  // namespace chromeos::mahi
