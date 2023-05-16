// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_generation_popup_view_views.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {

using password_manager::features::PasswordGenerationVariation;

// The max width prevents the popup from growing too much when the password
// field is too long.
constexpr int kPasswordGenerationMaxWidth = 480;

// Fixed dimensions of the minimized version of the popup, displayed in
// `kPasswordStrengthIndicatorWithMinimizedState` experiment when the typed
// password is weak and has over 5 characters.
constexpr int kMinimizedPopupWidth = 42;
constexpr int kMinimizedPopupHeight = 32;

// The default icon size used in the password generation drop down.
constexpr int kIconSize = 16;

// Adds space between child views. The `view`'s LayoutManager  must be a
// BoxLayout.
void AddSpacerWithSize(int spacer_width, bool resize, views::View* view) {
  auto spacer = std::make_unique<views::View>();
  spacer->SetPreferredSize(gfx::Size(spacer_width, /*height=*/1));
  static_cast<views::BoxLayout*>(view->GetLayoutManager())
      ->SetFlexForView(view->AddChildView(std::move(spacer)),
                       /*flex=*/resize ? 1 : 0,
                       /*use_min_size=*/true);
}

// Adds the password strength string and the warning icon children to the
// view.
std::unique_ptr<views::View> CreatePasswordStrengthView(
    const std::u16string& password_strength_text) {
  auto password_strength_view = std::make_unique<views::View>();

  auto warning_icon = std::make_unique<views::ImageView>();
  warning_icon->SetCanProcessEventsWithinSubtree(false);
  warning_icon->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kNotificationWarningIcon, ui::kColorAlertMediumSeverityIcon,
      kIconSize));
  password_strength_view->AddChildView(std::move(warning_icon));

  auto* layout = password_strength_view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Add space between the icon and the password strength string.
  AddSpacerWithSize(autofill::PopupBaseView::GetHorizontalPadding(), false,
                    password_strength_view.get());

  auto* password_strength_label =
      password_strength_view->AddChildView(std::make_unique<views::Label>(
          password_strength_text, views::style::CONTEXT_DIALOG_BODY_TEXT,
          views::style::STYLE_HIGHLIGHTED));
  password_strength_label->SetMultiLine(true);
  password_strength_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  return password_strength_view;
}

// Returns the message id of the help text which may differ depending on
// specific variation of `kPasswordGenerationExperiment` feature enabled.
int GetHelpTextMessageId() {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kPasswordGenerationExperiment)) {
    return IDS_PASSWORD_GENERATION_PROMPT_GOOGLE_PASSWORD_MANAGER;
  }

  switch (
      password_manager::features::kPasswordGenerationExperimentVariationParam
          .Get()) {
    case PasswordGenerationVariation::kTrustedAdvice:
      return IDS_PASSWORD_GENERATION_HELP_TEXT_TRUSTED_ADVICE;
    case PasswordGenerationVariation::kSafetyFirst:
      return IDS_PASSWORD_GENERATION_HELP_TEXT_SAFETY_FIRST;
    case PasswordGenerationVariation::kTrySomethingNew:
      return IDS_PASSWORD_GENERATION_HELP_TEXT_TRY_SOMETHING_NEW;
    case PasswordGenerationVariation::kConvenience:
      return IDS_PASSWORD_GENERATION_HELP_TEXT_CONVENIENCE;
  }
}

}  // namespace

// Class that shows the generated password and associated UI (currently an
// explanatory text).
class PasswordGenerationPopupViewViews::GeneratedPasswordBox
    : public views::View {
 public:
  METADATA_HEADER(GeneratedPasswordBox);
  GeneratedPasswordBox() = default;
  ~GeneratedPasswordBox() override = default;

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    if (!controller_) {
      return;
    }

    node_data->role = ax::mojom::Role::kListBoxOption;
    node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                controller_->password_selected());
    node_data->SetNameChecked(base::JoinString(
        {controller_->SuggestedText(), controller_->password()}, u" "));
    const std::u16string help_text = l10n_util::GetStringFUTF16(
        IDS_PASSWORD_GENERATION_PROMPT_GOOGLE_PASSWORD_MANAGER,
        l10n_util::GetStringUTF16(
            IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT),
        controller_->GetPrimaryAccountEmail());

    node_data->SetDescription(help_text);
  }

  // Fills the view with strings provided by |controller|.
  void Init(base::WeakPtr<PasswordGenerationPopupController> controller);

  void UpdateGeneratedPassword(const std::u16string& password) {
    password_label_->SetText(password);
  }

  void UpdateBackground(ui::ColorId color) {
    SetBackground(views::CreateThemedSolidBackground(color));
    // Setting a background color on the labels may change the text color to
    // improve contrast.
    password_label_->SetBackgroundColorId(color);
    suggestion_label_->SetBackgroundColorId(color);
  }

  void reset_controller() { controller_ = nullptr; }

 private:
  // Implements the View interface.
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  raw_ptr<views::Label> suggestion_label_ = nullptr;
  raw_ptr<views::Label> password_label_ = nullptr;
  base::WeakPtr<PasswordGenerationPopupController> controller_ = nullptr;
};

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::Init(
    base::WeakPtr<PasswordGenerationPopupController> controller) {
  // Make sure we only receive enter/exit events when the mouse enters the whole
  // view, even if it is entering/exiting a child view. This is needed to
  // prevent the background highlight of the password box disappearing when
  // entering the key icon view (see crbug.com/1393991).
  SetNotifyEnterExitOnChild(true);

  controller_ = controller;
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          GooglePasswordManagerVectorIcon(), ui::kColorIcon, kIconSize)));
  AddSpacerWithSize(PopupBaseView::GetHorizontalPadding(),
                    /*resize=*/false, this);

  suggestion_label_ = AddChildView(std::make_unique<views::Label>(
      controller_->SuggestedText(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      controller_->state() ==
              PasswordGenerationPopupController::kOfferGeneration
          ? views::style::STYLE_PRIMARY
          : views::style::STYLE_SECONDARY));

  AddSpacerWithSize(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_BETWEEN_PRIMARY_AND_SECONDARY_LABELS_HORIZONTAL),
      /*resize=*/true, this);

  DCHECK(!password_label_);
  password_label_ = AddChildView(std::make_unique<views::Label>(
      controller_->password(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      STYLE_SECONDARY_MONOSPACED));
  layout->SetFlexForView(password_label_, 1);
}

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::OnMouseEntered(
    const ui::MouseEvent& event) {
  if (controller_)
    controller_->SetSelected();
}

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::OnMouseExited(
    const ui::MouseEvent& event) {
  if (controller_)
    controller_->SelectionCleared();
}

bool PasswordGenerationPopupViewViews::GeneratedPasswordBox::OnMousePressed(
    const ui::MouseEvent& event) {
  return event.GetClickCount() == 1;
}

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::OnMouseReleased(
    const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && controller_)
    controller_->PasswordAccepted();
}

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::OnGestureEvent(
    ui::GestureEvent* event) {
  if (!controller_)
    return;
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      controller_->SetSelected();
      break;
    case ui::ET_GESTURE_TAP:
      controller_->PasswordAccepted();
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_END:
      controller_->SelectionCleared();
      break;
    default:
      return;
  }
}

BEGIN_METADATA(PasswordGenerationPopupViewViews,
               GeneratedPasswordBox,
               views::View)
END_METADATA

PasswordGenerationPopupViewViews::PasswordGenerationPopupViewViews(
    base::WeakPtr<PasswordGenerationPopupController> controller,
    views::Widget* parent_widget)
    : PopupBaseView(controller, parent_widget), controller_(controller) {
  CreateLayoutAndChildren();
}

PasswordGenerationPopupViewViews::~PasswordGenerationPopupViewViews() = default;

bool PasswordGenerationPopupViewViews::Show() {
  return DoShow();
}

void PasswordGenerationPopupViewViews::Hide() {
  // The controller is no longer valid after it hides us.
  controller_ = nullptr;
  if (FullPopupVisible()) {
    password_view_->reset_controller();
  }

  DoHide();
}

void PasswordGenerationPopupViewViews::UpdateState() {
  password_view_ = nullptr;
  RemoveAllChildViews();
  CreateLayoutAndChildren();
}

void PasswordGenerationPopupViewViews::UpdateGeneratedPasswordValue() {
  if (FullPopupVisible()) {
    password_view_->UpdateGeneratedPassword(controller_->password());
  }
  Layout();
}

bool PasswordGenerationPopupViewViews::UpdateBoundsAndRedrawPopup() {
  return DoUpdateBoundsAndRedrawPopup();
}

void PasswordGenerationPopupViewViews::PasswordSelectionUpdated() {
  DCHECK(FullPopupVisible());

  if (controller_->password_selected()) {
    DCHECK(this->password_view_);
    NotifyAXSelection(*this->password_view_);
  }

  if (!GetWidget())
    return;

  password_view_->UpdateBackground(controller_->password_selected()
                                       ? ui::kColorDropdownBackgroundSelected
                                       : ui::kColorDropdownBackground);
  SchedulePaint();
}

void PasswordGenerationPopupViewViews::CreateLayoutAndChildren() {
  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorDropdownBackground));
  if (controller_->IsStateMinimized()) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    auto warning_icon = std::make_unique<views::ImageView>();
    warning_icon->SetImage(ui::ImageModel::FromVectorIcon(
        vector_icons::kNotificationWarningIcon,
        ui::kColorAlertMediumSeverityIcon, kIconSize));
    AddChildView(std::move(warning_icon));
    return;
  }

  views::BoxLayout* box_layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int kVerticalPadding =
      provider->GetDistanceMetric(DISTANCE_TOAST_LABEL_VERTICAL);
  const int kHorizontalMargin =
      provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL);

  if (controller_->IsUserTypedPasswordWeak()) {
    auto* password_strength_view = AddChildView(CreatePasswordStrengthView(
        l10n_util::GetStringUTF16(IDS_PASSWORD_WEAKNESS_INDICATOR)));
    password_strength_view->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(kVerticalPadding, kHorizontalMargin)));
  }

  auto password_view = std::make_unique<GeneratedPasswordBox>();
  password_view->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kVerticalPadding, kHorizontalMargin)));
  password_view->Init(controller_);
  password_view_ = AddChildView(std::move(password_view));
  PasswordSelectionUpdated();

  AddChildView(views::Builder<views::Separator>()
                   .SetOrientation(views::Separator::Orientation::kHorizontal)
                   .SetColorId(ui::kColorMenuSeparator)
                   .Build());

  base::RepeatingClosure open_password_manager_closure = base::BindRepeating(
      [](PasswordGenerationPopupViewViews* view) {
        if (!view->controller_)
          return;
        view->controller_->OnGooglePasswordManagerLinkClicked();
      },
      base::Unretained(this));

  views::StyledLabel* help_label =
      AddChildView(CreateGooglePasswordManagerLabel(
          GetHelpTextMessageId(),
          /*link_message_id=*/
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
          controller_->GetPrimaryAccountEmail(),
          open_password_manager_closure));

  help_label->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kVerticalPadding, kHorizontalMargin)));
  help_label->SetDisplayedOnBackgroundColor(ui::kColorBubbleFooterBackground);
}

bool PasswordGenerationPopupViewViews::FullPopupVisible() const {
  return password_view_;
}

void PasswordGenerationPopupViewViews::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  // TODO(crbug.com/1404297): kListBox is used for the same reason as in
  // `autofill::PopupViewViews`. See crrev.com/c/2545285 for details.
  // Consider using a more appropriate role (e.g. kMenuListPopup or similar).
  node_data->role = ax::mojom::Role::kListBox;

  if (!controller_) {
    node_data->AddState(ax::mojom::State::kCollapsed);
    node_data->AddState(ax::mojom::State::kInvisible);
    return;
  }

  node_data->AddState(ax::mojom::State::kExpanded);
}

gfx::Size PasswordGenerationPopupViewViews::CalculatePreferredSize() const {
  if (!FullPopupVisible()) {
    return gfx::Size(kMinimizedPopupWidth, kMinimizedPopupHeight);
  }

  int width =
      std::max(password_view_->GetPreferredSize().width(),
               gfx::ToEnclosingRect(controller_->element_bounds()).width());
  width = std::min(width, kPasswordGenerationMaxWidth);
  return gfx::Size(width, GetHeightForWidth(width));
}

PasswordGenerationPopupView* PasswordGenerationPopupView::Create(
    base::WeakPtr<PasswordGenerationPopupController> controller) {
  if (!controller->container_view())
    return nullptr;

  views::Widget* observing_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          controller->container_view());

  return new PasswordGenerationPopupViewViews(controller, observing_widget);
}

BEGIN_METADATA(PasswordGenerationPopupViewViews, autofill::PopupBaseView)
END_METADATA
