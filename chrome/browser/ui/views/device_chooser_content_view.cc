// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/device_chooser_content_view.h"

#include <string>

#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

DeviceChooserContentView::DeviceChooserContentView(
    views::TableViewObserver* table_view_observer,
    std::unique_ptr<permissions::ChooserController> chooser_controller)
    : chooser_controller_(std::move(chooser_controller)) {
  chooser_controller_->set_view(this);

  SetPreferredSize(gfx::Size(402, 320));

  if (chooser_controller_->ShouldShowSelectAllCheckbox()) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    auto select_all_view = std::make_unique<views::Checkbox>(
        chooser_controller_->GetSelectAllCheckboxLabel());
    select_all_view->SetVisible(false);
    select_all_subscription_ = select_all_view->AddCheckedChangedCallback(
        base::BindRepeating(&DeviceChooserContentView::SelectAllCheckboxChanged,
                            base::Unretained(this)));
    select_all_view_ = AddChildView(std::move(select_all_view));
  } else {
    // FillLayout is the default. There will only be the ScrollView,
    // therefore there's no point to have a BoxLayout.
    SetUseDefaultFillLayout(true);
  }

  std::vector<ui::TableColumn> table_columns = {ui::TableColumn()};
  auto table_view = std::make_unique<views::TableView>(
      this, table_columns,
      chooser_controller_->ShouldShowIconBeforeText()
          ? views::TableType::kIconAndText
          : views::TableType::kTextOnly,
      !chooser_controller_->AllowMultipleSelection() /* single_selection */);
  table_view_ = table_view.get();
  table_view->SetSelectOnRemove(false);
  table_view->set_observer(table_view_observer);
  table_view->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(
          IDS_DEVICE_CHOOSER_ACCNAME_COMPATIBLE_DEVICES_LIST),
      ax::mojom::NameFrom::kAttribute);

  table_parent_ = AddChildView(
      views::TableView::CreateScrollViewWithTable(std::move(table_view)));
  if (chooser_controller_->ShouldShowSelectAllCheckbox()) {
    // This will be using the BoxLayout manager.
    // Set min and max height, otherwise CalculatePreferredSize() will be
    // called, returning 0, 0 always.
    table_parent_->ClipHeightTo(320, 320);
  }

  const auto add_centering_view = [this](auto view) {
    auto* container = AddChildView(std::make_unique<views::View>());
    auto* layout =
        container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->set_inside_border_insets(gfx::Insets::VH(0, 6));
    container->AddChildView(std::move(view));
    return container;
  };

  // Label that explains that there are no devices to choose from.
  auto no_options_help =
      std::make_unique<views::Label>(chooser_controller_->GetNoOptionsText());
  no_options_help->SetMultiLine(true);
  no_options_view_ = add_centering_view(std::move(no_options_help));

  // Link that explains that Bluetooth must be turned on.
  if (chooser_controller_->ShouldShowAdapterOffView()) {
    std::u16string link_text = l10n_util::GetStringUTF16(
        chooser_controller_->GetTurnAdapterOnLinkTextMessageId());
    size_t offset = 0;
    std::u16string text = l10n_util::GetStringFUTF16(
        chooser_controller_->GetAdapterOffMessageId(), link_text, &offset);
    auto adapter_off_help = std::make_unique<views::StyledLabel>();
    adapter_off_help->SetText(text);
    adapter_off_help->AddStyleRange(
        gfx::Range(offset, offset + link_text.size()),
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &permissions::ChooserController::OpenAdapterOffHelpUrl,
            base::Unretained(chooser_controller_.get()))));
    adapter_off_view_ = add_centering_view(std::move(adapter_off_help));
  }

  // Link that explains that OS Bluetooth permission must be granted.
  if (chooser_controller_->ShouldShowAdapterUnauthorizedView()) {
    std::u16string link_text = l10n_util::GetStringUTF16(
        chooser_controller_->GetAuthorizeBluetoothLinkTextMessageId());
    std::u16string text = l10n_util::GetStringFUTF16(
        chooser_controller_->GetBluetoothUnauthorizedMessageId(), link_text);
    size_t text_end = text.size();
    auto adapter_unauthorized_help = std::make_unique<views::StyledLabel>();
    adapter_unauthorized_help->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_CENTER);
    adapter_unauthorized_help->SetText(text);
    adapter_unauthorized_help->AddStyleRange(
        gfx::Range(text_end - link_text.size(), text_end),
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &permissions::ChooserController::OpenPermissionPreferences,
            base::Unretained(chooser_controller_.get()))));
    adapter_unauthorized_view_ =
        add_centering_view(std::move(adapter_unauthorized_help));
  }

  UpdateTableView();
}

DeviceChooserContentView::~DeviceChooserContentView() {
  chooser_controller_->set_view(nullptr);
  table_view_->set_observer(nullptr);
  table_view_->SetModel(nullptr);
}

gfx::Size DeviceChooserContentView::GetMinimumSize() const {
  // Let the dialog shrink when its parent is smaller than the preferred size.
  return gfx::Size();
}

size_t DeviceChooserContentView::RowCount() {
  return chooser_controller_->NumOptions();
}

std::u16string DeviceChooserContentView::GetText(size_t row, int column_id) {
  DCHECK_LT(row, RowCount());
  std::u16string text = chooser_controller_->GetOption(row);
  return chooser_controller_->IsPaired(row)
             ? l10n_util::GetStringFUTF16(
                   IDS_DEVICE_CHOOSER_DEVICE_NAME_AND_PAIRED_STATUS_TEXT, text)
             : text;
}

void DeviceChooserContentView::SetObserver(ui::TableModelObserver* observer) {}

ui::ImageModel DeviceChooserContentView::GetIcon(size_t row) {
  DCHECK(chooser_controller_->ShouldShowIconBeforeText());
  DCHECK_LT(row, RowCount());

  if (chooser_controller_->IsConnected(row)) {
    return ui::ImageModel::FromVectorIcon(vector_icons::kBluetoothConnectedIcon,
                                          ui::kColorIcon,
                                          TableModel::kIconSize);
  }

  int level = chooser_controller_->GetSignalStrengthLevel(row);
  if (level == -1)
    return ui::ImageModel();

  constexpr int kSignalStrengthLevelImageIds[5] = {
      IDR_SIGNAL_0_BAR, IDR_SIGNAL_1_BAR, IDR_SIGNAL_2_BAR, IDR_SIGNAL_3_BAR,
      IDR_SIGNAL_4_BAR};
  DCHECK_GE(level, 0);
  DCHECK_LT(static_cast<size_t>(level),
            std::size(kSignalStrengthLevelImageIds));
  return ui::ImageModel::FromImageSkia(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          kSignalStrengthLevelImageIds[level]));
}

void DeviceChooserContentView::OnOptionsInitialized() {
  is_initialized_ = true;
  table_view_->OnModelChanged();
  UpdateTableView();
  HideThrobber();
}

void DeviceChooserContentView::OnOptionAdded(size_t index) {
  is_initialized_ = true;
  table_view_->OnItemsAdded(index, 1);
  UpdateTableView();
}

void DeviceChooserContentView::OnOptionRemoved(size_t index) {
  table_view_->OnItemsRemoved(index, 1);
  UpdateTableView();
}

void DeviceChooserContentView::OnOptionUpdated(size_t index) {
  table_view_->OnItemsChanged(index, 1);
  UpdateTableView();
}

void DeviceChooserContentView::OnAdapterEnabledChanged(bool enabled) {
  // No row is selected since the adapter status has changed.
  // This will also disable the OK button if it was enabled because
  // of a previously selected row.
  table_view_->Select(std::nullopt);
  adapter_enabled_ = enabled;
  UpdateTableView();

  if (re_scan_button_) {
    ShowReScanButton(enabled);
  }

  if (GetWidget() && GetWidget()->GetRootView())
    GetWidget()->GetRootView()->DeprecatedLayoutImmediately();
}

void DeviceChooserContentView::OnAdapterAuthorizationChanged(bool authorized) {
  // No row is selected since we are not authorized to get device info anyway.
  table_view_->Select(std::nullopt);
  adapter_authorized_ = authorized;
  UpdateTableView();

  if (re_scan_button_) {
    ShowReScanButton(authorized);
  }
}

void DeviceChooserContentView::OnRefreshStateChanged(bool refreshing) {
  if (refreshing) {
    // No row is selected since the chooser is refreshing. This will also
    // disable the OK button if it was enabled because of a previously
    // selected row.
    table_view_->Select(std::nullopt);
    UpdateTableView();
  }

  if (refreshing)
    ShowThrobber();
  else
    ShowReScanButton(/*enable=*/true);

  if (GetWidget() && GetWidget()->GetRootView())
    GetWidget()->GetRootView()->DeprecatedLayoutImmediately();
}

std::u16string DeviceChooserContentView::GetWindowTitle() const {
  return chooser_controller_->GetTitle();
}

std::unique_ptr<views::View> DeviceChooserContentView::CreateExtraView() {
  auto container = std::make_unique<views::View>();
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  container->SetLayoutManager(std::move(layout))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  if (chooser_controller_->ShouldShowHelpButton()) {
    std::unique_ptr<views::ImageButton> help_button;
    help_button = views::ImageButton::CreateIconButton(
        base::BindRepeating(&permissions::ChooserController::OpenHelpCenterUrl,
                            base::Unretained(chooser_controller_.get())),
        vector_icons::kHelpOutlineIcon,
        l10n_util::GetStringUTF16(IDS_LEARN_MORE),
        views::ImageButton::MaterialIconStyle::kLarge,
        views::LayoutProvider::Get()->GetInsetsMetric(
            views::INSETS_VECTOR_IMAGE_BUTTON));
    help_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
    container->AddChildView(std::move(help_button));
  }

  auto* throbber_container =
      container->AddChildView(std::make_unique<views::View>());
  auto* throbber_layout =
      throbber_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  throbber_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  throbber_layout->set_between_child_spacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL));

  throbber_ =
      throbber_container->AddChildView(std::make_unique<views::Throbber>());

  auto throbber_strings = chooser_controller_->GetThrobberLabelAndTooltip();
  auto throbber_label = std::make_unique<views::Label>(
      throbber_strings.first, views::style::CONTEXT_LABEL,
      views::style::STYLE_DISABLED);
  throbber_label->SetTooltipText(throbber_strings.second);
  throbber_label_ = throbber_container->AddChildView(std::move(throbber_label));

  if (chooser_controller_->ShouldShowReScanButton()) {
    auto* rescan_container =
        container->AddChildView(std::make_unique<views::View>());
    rescan_container
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal))
        ->set_cross_axis_alignment(
            views::BoxLayout::CrossAxisAlignment::kCenter);

    auto re_scan_button = std::make_unique<views::MdTextButton>(
        views::Button::PressedCallback(),
        l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN));
    re_scan_button->SetCallback(base::BindRepeating(
        [](views::MdTextButton* button,
           permissions::ChooserController* chooser_controller) {
          // Refreshing will cause the table view to yield focus, which will
          // land on the help button. Instead, briefly let the rescan button
          // take focus. When it hides itself, focus will advance to the
          // "Cancel" button as desired.
          button->RequestFocus();
          chooser_controller->RefreshOptions();
        },
        re_scan_button.get(), chooser_controller_.get()));
    re_scan_button->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN_TOOLTIP));
    re_scan_button_ = rescan_container->AddChildView(std::move(re_scan_button));
  }

  // Enable the throbber by default until OnOptionsInitialized() is called.
  ShowThrobber();

  return container;
}

bool DeviceChooserContentView::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  return chooser_controller_->BothButtonsAlwaysEnabled() ||
         button != ui::mojom::DialogButton::kOk ||
         !table_view_->selection_model().empty();
}

void DeviceChooserContentView::Accept() {
  std::vector<size_t> indices(
      table_view_->selection_model().selected_indices().begin(),
      table_view_->selection_model().selected_indices().end());
  chooser_controller_->Select(indices);
}

void DeviceChooserContentView::Cancel() {
  chooser_controller_->Cancel();
}

void DeviceChooserContentView::Close() {
  chooser_controller_->Close();
}

void DeviceChooserContentView::UpdateTableView() {
  bool has_options = adapter_enabled_ && adapter_authorized_ && RowCount() > 0;

  if (select_all_view_) {
    select_all_view_->SetVisible(
        has_options && chooser_controller_->ShouldShowSelectAllCheckbox());
  }

  table_parent_->SetVisible(has_options);
  table_view_->SetEnabled(has_options &&
                          !chooser_controller_->TableViewAlwaysDisabled());

  // The "No devices found" label should not show until enumeration has
  // completed or this widget has received focus, in order to prevent a brief
  // flash of incorrect text that could be read by a screen reader.
  if (!is_initialized_ && GetWidget() &&
      GetWidget()->GetFocusManager()->GetFocusedView()) {
    is_initialized_ = true;
  }
  no_options_view_->SetVisible(RowCount() == 0 && is_initialized_ &&
                               adapter_enabled_ && adapter_authorized_);
  if (adapter_off_view_) {
    adapter_off_view_->SetVisible(!adapter_enabled_ && adapter_authorized_);
  }
  if (adapter_unauthorized_view_) {
    adapter_unauthorized_view_->SetVisible(!adapter_authorized_);
  }
  if (!adapter_enabled_ || !adapter_authorized_) {
    HideThrobber();
  }
}

void DeviceChooserContentView::SelectAllCheckboxChanged() {
  DCHECK(select_all_view_ && table_view_);
  table_view_->SetSelectionAll(/*select=*/select_all_view_->GetChecked());
}

void DeviceChooserContentView::ShowThrobber() {
  if (re_scan_button_)
    re_scan_button_->SetVisible(false);

  throbber_->SetVisible(true);
  throbber_label_->SetVisible(true);
  throbber_->Start();
}

void DeviceChooserContentView::HideThrobber() {
  throbber_->SetVisible(false);
  throbber_label_->SetVisible(false);
  throbber_->Stop();
}

void DeviceChooserContentView::ShowReScanButton(bool enabled) {
  DCHECK(re_scan_button_);
  re_scan_button_->SetVisible(true);
  re_scan_button_->SetEnabled(enabled);
  throbber_->Stop();
  throbber_->SetVisible(false);
  throbber_label_->SetVisible(false);
}

views::LabelButton* DeviceChooserContentView::ReScanButtonForTesting() {
  return re_scan_button_;
}

views::Throbber* DeviceChooserContentView::ThrobberForTesting() {
  return throbber_;
}

views::Label* DeviceChooserContentView::ThrobberLabelForTesting() {
  return throbber_label_;
}

BEGIN_METADATA(DeviceChooserContentView)
ADD_READONLY_PROPERTY_METADATA(std::u16string, WindowTitle)
END_METADATA
