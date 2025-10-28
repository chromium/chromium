// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/protocol_handler_picker_dialog.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(
    kProtocolHandlerPickerDialogRememberSelectionCheckboxId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kProtocolHandlerPickerDialogOkButtonId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kProtocolHandlerPickerDialogSelectionId);

namespace web_app {
namespace {

constexpr int kAppNameLabelHeight = 20;

constexpr int kCheckIconSize = 20;

enum class BackgroundType { kDefault, kSelected, kHovered };

std::unique_ptr<views::Background> CreateBackgroundForProtocolHandlerRow(
    BackgroundType type) {
  auto token = [&] {
    switch (type) {
      case BackgroundType::kDefault:
        return cros_tokens::kCrosSysAppBaseShaded;
      case BackgroundType::kSelected:
        return cros_tokens::kCrosSysSystemPrimaryContainer;
      case BackgroundType::kHovered:
        return cros_tokens::kCrosSysHoverOnSubtle;
    }
  }();

  return views::CreateRoundedRectBackground(
      token, views::LayoutProvider::Get()->GetCornerRadiusMetric(
                 views::ShapeContextTokens::kDialogRadius));
}

std::u16string GetDialogTitle(
    const GURL& protocol_url,
    const web_app::ProtocolHandlerPickerDialogEntries& apps) {
  std::u16string protocol_scheme = base::UTF8ToUTF16(protocol_url.GetScheme()) +
                                   url::kStandardSchemeSeparator16;
  return apps.size() == 1
             ? l10n_util::GetStringFUTF16(
                   IDS_PROTOCOL_HANDLER_PICKER_TITLE_SINGLE_APP,
                   protocol_scheme,
                   gfx::TruncateString(apps[0].app_name, /*length=*/30,
                                       gfx::WORD_BREAK))
             : l10n_util::GetStringFUTF16(
                   IDS_PROTOCOL_HANDLER_PICKER_TITLE_MULTIPLE_APPS,
                   protocol_scheme);
}

ui::DialogModelLabel GetDialogParagraph(
    const std::optional<std::u16string>& initiator_display_name) {
  return initiator_display_name
             ? ui::DialogModelLabel::CreateWithReplacement(
                   IDS_PROTOCOL_HANDLER_PICKER_PARAGRAPH_WITH_ORIGIN,
                   ui::DialogModelLabel::CreatePlainText(
                       *initiator_display_name))
             : ui::DialogModelLabel(
                   IDS_PROTOCOL_HANDLER_PICKER_PARAGRAPH_GENERIC);
}

class SelectionView : public views::ScrollView,
                      public ProtocolHandlerPickerSelectionRowView::Delegate {
  METADATA_HEADER(SelectionView, views::ScrollView)

 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnSelected(const std::string& app_id) = 0;
  };

  SelectionView(const ProtocolHandlerPickerDialogEntries& apps,
                Delegate& delegate);
  ~SelectionView() override = default;

 private:
  void OnRowClicked(ProtocolHandlerPickerSelectionRowView* row) override;

  static constexpr int kProtocolHandlerRowGroupID = 1;

  raw_ptr<ProtocolHandlerPickerSelectionRowView> selected_row_ = nullptr;
  const raw_ref<Delegate> delegate_;
};

SelectionView::SelectionView(const ProtocolHandlerPickerDialogEntries& apps,
                             Delegate& delegate)
    : delegate_(delegate) {
  auto* scrollable_container = SetContents(
      views::Builder<views::View>()
          .SetLayoutManager(std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kVertical, gfx::Insets(),
              views::LayoutProvider::Get()->GetDistanceMetric(
                  views::DISTANCE_RELATED_CONTROL_VERTICAL)))
          .SetProperty(views::kElementIdentifierKey,
                       kProtocolHandlerPickerDialogSelectionId)
          .SetAccessibleRole(ax::mojom::Role::kList)
          .SetAccessibleName(l10n_util::GetStringUTF16(
              IDS_PROTOCOL_HANDLER_PICKER_APPLICATION_OPTIONS_ACCESSIBLE_NAME))
          .Build());

  scrollable_container->GetViewAccessibility().SetSetSize(apps.size());
  if (apps.size() == 1) {
    // For the single-app case, the app is already pre-selected for the user
    // (and has no check icon).
    auto* row = scrollable_container->AddChildView(
        std::make_unique<ProtocolHandlerPickerSelectionRowView>(
            apps[0], *this, /*include_check_icon=*/false));
    row->SetGroup(kProtocolHandlerRowGroupID);
    row->SetSelected(true);

    auto& view_ax = row->GetViewAccessibility();
    // Must be 1-based.
    view_ax.SetPosInSet(1);
    view_ax.SetSetSize(1);
    selected_row_ = row;
  } else {
    for (uint32_t i = 0; i < apps.size(); i++) {
      auto* app_row = scrollable_container->AddChildView(
          std::make_unique<ProtocolHandlerPickerSelectionRowView>(apps[i],
                                                                  *this));
      app_row->SetGroup(kProtocolHandlerRowGroupID);

      auto& view_ax = app_row->GetViewAccessibility();
      // Must be 1-based.
      view_ax.SetPosInSet(i + 1);
      view_ax.SetSetSize(apps.size());
    }
  }

  // For <= 3 apps, no scrolling happens; for >= 4 apps, the fourth and
  // further apps will be placed in a cropped scroll view (note that the
  // fourth app won't be fully visible due to the 8px gap between app
  // entries).
  const int row_height =
      contents()->children().front()->GetPreferredSize().height();
  ClipHeightTo(row_height, 4 * row_height);
}

void SelectionView::OnRowClicked(ProtocolHandlerPickerSelectionRowView* row) {
  if (selected_row_ == row) {
    return;
  }
  if (selected_row_) {
    // Unselect previously selected row.
    selected_row_->SetSelected(false);
  }
  selected_row_ = row;
  selected_row_->SetSelected(true);
  // Select the new row and inform the delegate.
  delegate_->OnSelected(selected_row_->app_id());
}

BEGIN_METADATA(SelectionView)
END_METADATA

class ProtocolHandlerPickerDelegate : public ui::DialogModelDelegate,
                                      public SelectionView::Delegate {
 public:
  explicit ProtocolHandlerPickerDelegate(OnPreferredHandlerSelected callback)
      : callback_(std::move(callback)) {}

  ~ProtocolHandlerPickerDelegate() override = default;

  void OnSelected(const std::string& app_id) override {
    selected_app_id_ = app_id;
    ui::DialogModel::Button* ok_button = dialog_model()->GetButtonByUniqueId(
        kProtocolHandlerPickerDialogOkButtonId);
    dialog_model()->SetButtonEnabled(ok_button, true);
  }

  void OnAccept() {
    bool remember_choice =
        dialog_model()
            ->GetCheckboxByUniqueId(
                kProtocolHandlerPickerDialogRememberSelectionCheckboxId)
            ->is_checked();

    CHECK(selected_app_id_);
    CHECK(callback_);
    std::move(callback_).Run(*selected_app_id_, remember_choice);
  }

 private:
  std::optional<std::string> selected_app_id_;
  OnPreferredHandlerSelected callback_;
};

}  // namespace

ProtocolHandlerPickerSelectionRowView::ProtocolHandlerPickerSelectionRowView(
    const ProtocolHandlerPickerDialogEntry& app,
    Delegate& delegate,
    bool include_check_icon)
    : Button(base::BindRepeating(
          &ProtocolHandlerPickerSelectionRowView::OnRowClicked,
          base::Unretained(this))),
      app_id_(app.app_id),
      delegate_(delegate) {
  auto layout_manager = std::make_unique<views::FlexLayout>();
  layout_manager->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // Defines the left margin for child elements.
  const auto left_gap =
      gfx::Insets().set_left(views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  auto builder =
      views::Builder<ProtocolHandlerPickerSelectionRowView>(this)
          .SetBorder(views::CreateEmptyBorder(
              views::LayoutProvider::Get()->GetDistanceMetric(
                  views::DISTANCE_UNRELATED_CONTROL_HORIZONTAL)))
          .SetLayoutManager(std::move(layout_manager))
          .AddChildren(
              // App icon
              views::Builder<views::ImageView>().SetImage(app.icon),
              // App label
              views::Builder<views::Label>()
                  .SetAccessibleRole(ax::mojom::Role::kSectionHeader)
                  .SetText(app.app_name)
                  .SetElideBehavior(gfx::ELIDE_TAIL)
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::LayoutOrientation::kHorizontal,
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded))
                  .SetFontList(gfx::FontList("Roboto, 14px")
                                   .DeriveWithWeight(gfx::Font::Weight::MEDIUM))
                  .SetLineHeight(kAppNameLabelHeight)
                  .SetProperty(views::kMarginsKey, left_gap))
          .SetAccessibleRole(ax::mojom::Role::kMenuItemRadio)
          .SetAccessibleName(app.app_name)
          .SetCheckedState(ax::mojom::CheckedState::kFalse)
          .SetInstallFocusRingOnFocus(false)
          .SetBackground(
              CreateBackgroundForProtocolHandlerRow(BackgroundType::kDefault));

  if (include_check_icon) {
    // Check icon
    builder.AddChild(
        views::Builder<views::ImageView>()
            .SetImage(ui::ImageModel::FromVectorIcon(
                vector_icons::kCheckCircleIcon,
                cros_tokens::kCrosSysOnPrimaryContainer, kCheckIconSize))
            .SetVisible(false)
            .SetProperty(views::kMarginsKey, left_gap)
            .CopyAddressTo(&check_icon_));
  }

  std::move(builder).BuildChildren();
}

ProtocolHandlerPickerSelectionRowView::
    ~ProtocolHandlerPickerSelectionRowView() = default;

void ProtocolHandlerPickerSelectionRowView::SetSelected(bool selected) {
  if (is_selected_ == selected) {
    return;
  }
  is_selected_ = selected;
  if (check_icon_) {
    check_icon_->SetVisible(is_selected_);
  }
  SetCheckedState(is_selected_ ? ax::mojom::CheckedState::kTrue
                               : ax::mojom::CheckedState::kFalse);
  UpdateBackground();
}

bool ProtocolHandlerPickerSelectionRowView::IsSelected() const {
  return is_selected_;
}

void ProtocolHandlerPickerSelectionRowView::SetCheckedState(
    ax::mojom::CheckedState state) {
  GetViewAccessibility().SetCheckedState(state);
}

void ProtocolHandlerPickerSelectionRowView::OnRowClicked() {
  delegate_->OnRowClicked(this);
}

void ProtocolHandlerPickerSelectionRowView::StateChanged(
    ButtonState old_state) {
  UpdateBackground();
}

void ProtocolHandlerPickerSelectionRowView::UpdateBackground() {
  // Selection state always takes precedence.
  if (is_selected_) {
    SetBackground(
        CreateBackgroundForProtocolHandlerRow(BackgroundType::kSelected));
    return;
  }

  // If not selected, check for hover state.
  if (GetState() == STATE_HOVERED) {
    SetBackground(
        CreateBackgroundForProtocolHandlerRow(BackgroundType::kHovered));
  } else {
    SetBackground(
        CreateBackgroundForProtocolHandlerRow(BackgroundType::kDefault));
  }
}

BEGIN_METADATA(ProtocolHandlerPickerSelectionRowView)
END_METADATA

std::unique_ptr<ui::DialogModel> CreateProtocolHandlerPickerDialog(
    const GURL& protocol_url,
    const ProtocolHandlerPickerDialogEntries& apps,
    const std::optional<std::u16string>& initiator_display_name,
    OnPreferredHandlerSelected callback) {
  CHECK(!apps.empty());

  auto delegate =
      std::make_unique<ProtocolHandlerPickerDelegate>(std::move(callback));
  auto* delegate_ptr = delegate.get();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(delegate))
          .SetInternalName("ProtocolHandlerPickerDialog")
          .SetTitle(GetDialogTitle(protocol_url, apps))
          .AddParagraph(
              ui::DialogModelLabel(GetDialogParagraph(initiator_display_name)))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  std::make_unique<SelectionView>(apps, *delegate_ptr),
                  views::BubbleDialogModelHost::FieldType::kControl))
          .AddCheckbox(
              kProtocolHandlerPickerDialogRememberSelectionCheckboxId,
              ui::DialogModelLabel::CreateWithReplacement(
                  IDS_PROTOCOL_HANDLER_PICKER_DIALOG_ALWAYS_OPEN_IN_THIS_APP,
                  ui::DialogModelLabel::CreatePlainText(
                      base::UTF8ToUTF16(protocol_url.scheme()) +
                      url::kStandardSchemeSeparator16)))
          .AddCancelButton(base::DoNothing())
          .AddOkButton(base::BindOnce(&ProtocolHandlerPickerDelegate::OnAccept,
                                      base::Unretained(delegate_ptr)),
                       ui::DialogModel::Button::Params()
                           .SetEnabled(false)
                           .SetId(kProtocolHandlerPickerDialogOkButtonId)
                           .SetLabel(l10n_util::GetStringUTF16(IDS_OPEN)))
          .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
          .Build();

  if (apps.size() == 1) {
    // Enable the button and set the default callback param to the only app id.
    delegate_ptr->OnSelected(apps[0].app_id);
  }
  return dialog_model;
}

}  // namespace web_app
