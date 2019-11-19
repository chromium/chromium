// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/device_chooser_content_view.h"

#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/resources/grit/ui_resources.h"
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
#include "ui/views/widget/widget.h"

namespace {

constexpr int kHelpButtonTag = 1;
constexpr int kReScanButtonTag = 2;

}  // namespace

class BluetoothStatusContainer : public views::View {
 public:
  explicit BluetoothStatusContainer(views::ButtonListener* listener);

  void ShowScanningLabelAndThrobber();
  void ShowReScanButton(bool enabled);

 private:
  friend class DeviceChooserContentView;

  views::LabelButton* re_scan_button_;
  views::Throbber* throbber_;
  views::Label* scanning_label_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothStatusContainer);
};

BluetoothStatusContainer::BluetoothStatusContainer(
    views::ButtonListener* listener) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto* rescan_container = AddChildView(std::make_unique<views::View>());
  rescan_container
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  auto re_scan_button = views::MdTextButton::CreateSecondaryUiButton(
      listener,
      l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN));
  re_scan_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN_TOOLTIP));
  re_scan_button->SetFocusForPlatform();
  re_scan_button->set_tag(kReScanButtonTag);
  re_scan_button_ = rescan_container->AddChildView(std::move(re_scan_button));

  auto* scan_container = AddChildView(std::make_unique<views::View>());
  auto* scan_layout =
      scan_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  scan_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  scan_layout->set_between_child_spacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL));

  throbber_ = scan_container->AddChildView(std::make_unique<views::Throbber>());

  auto scanning_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_DISABLED);
  scanning_label->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING_LABEL_TOOLTIP));
  scanning_label_ = scan_container->AddChildView(std::move(scanning_label));
}

void BluetoothStatusContainer::ShowScanningLabelAndThrobber() {
  re_scan_button_->SetVisible(false);
  throbber_->SetVisible(true);
  scanning_label_->SetVisible(true);
  throbber_->Start();
}

void BluetoothStatusContainer::ShowReScanButton(bool enabled) {
  re_scan_button_->SetVisible(true);
  re_scan_button_->SetEnabled(enabled);
  throbber_->Stop();
  throbber_->SetVisible(false);
  scanning_label_->SetVisible(false);
}

DeviceChooserContentView::DeviceChooserContentView(
    views::TableViewObserver* table_view_observer,
    std::unique_ptr<ChooserController> chooser_controller)
    : chooser_controller_(std::move(chooser_controller)) {
  chooser_controller_->set_view(this);

  SetPreferredSize({402, 320});
  SetLayoutManager(std::make_unique<views::FillLayout>());

  std::vector<ui::TableColumn> table_columns = {ui::TableColumn()};
  auto table_view = std::make_unique<views::TableView>(
      this, table_columns,
      chooser_controller_->ShouldShowIconBeforeText() ? views::ICON_AND_TEXT
                                                      : views::TEXT_ONLY,
      !chooser_controller_->AllowMultipleSelection() /* single_selection */);
  table_view_ = table_view.get();
  table_view->SetSelectOnRemove(false);
  table_view->set_observer(table_view_observer);

  table_parent_ = AddChildView(
      views::TableView::CreateScrollViewWithTable(std::move(table_view)));

  const auto add_centering_view = [this](auto view) {
    auto* container = AddChildView(std::make_unique<views::View>());
    auto* layout =
        container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->set_inside_border_insets(gfx::Insets(0, 6));
    container->AddChildView(std::move(view));
    return container;
  };

  auto no_options_help =
      std::make_unique<views::Label>(chooser_controller_->GetNoOptionsText());
  no_options_help->SetMultiLine(true);
  no_options_view_ = add_centering_view(std::move(no_options_help));

  base::string16 link_text = l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_TURN_ON_BLUETOOTH_LINK_TEXT);
  size_t offset = 0;
  base::string16 text = l10n_util::GetStringFUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_TURN_ADAPTER_OFF, link_text, &offset);
  auto adapter_off_help = std::make_unique<views::StyledLabel>(text, this);
  adapter_off_help->AddStyleRange(
      gfx::Range(0, link_text.size()),
      views::StyledLabel::RangeStyleInfo::CreateForLink());
  adapter_off_view_ = add_centering_view(std::move(adapter_off_help));

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

int DeviceChooserContentView::RowCount() {
  return base::checked_cast<int>(chooser_controller_->NumOptions());
}

base::string16 DeviceChooserContentView::GetText(int row, int column_id) {
  DCHECK_GE(row, 0);
  DCHECK_LT(row, RowCount());
  base::string16 text = chooser_controller_->GetOption(size_t{row});
  return chooser_controller_->IsPaired(row)
             ? l10n_util::GetStringFUTF16(
                   IDS_DEVICE_CHOOSER_DEVICE_NAME_AND_PAIRED_STATUS_TEXT, text)
             : text;
}

void DeviceChooserContentView::SetObserver(ui::TableModelObserver* observer) {}

gfx::ImageSkia DeviceChooserContentView::GetIcon(int row) {
  DCHECK(chooser_controller_->ShouldShowIconBeforeText());
  DCHECK_GE(row, 0);
  DCHECK_LT(row, RowCount());

  if (chooser_controller_->IsConnected(row)) {
    return gfx::CreateVectorIcon(vector_icons::kBluetoothConnectedIcon,
                                 TableModel::kIconSize, gfx::kChromeIconGrey);
  }

  int level = chooser_controller_->GetSignalStrengthLevel(row);
  if (level == -1)
    return gfx::ImageSkia();

  constexpr int kSignalStrengthLevelImageIds[5] = {
      IDR_SIGNAL_0_BAR, IDR_SIGNAL_1_BAR, IDR_SIGNAL_2_BAR, IDR_SIGNAL_3_BAR,
      IDR_SIGNAL_4_BAR};
  DCHECK_GE(level, 0);
  DCHECK_LT(size_t{level}, base::size(kSignalStrengthLevelImageIds));
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      kSignalStrengthLevelImageIds[level]);
}

void DeviceChooserContentView::OnOptionsInitialized() {
  table_view_->OnModelChanged();
  UpdateTableView();
}

void DeviceChooserContentView::OnOptionAdded(size_t index) {
  table_view_->OnItemsAdded(base::checked_cast<int>(index), 1);
  UpdateTableView();
}

void DeviceChooserContentView::OnOptionRemoved(size_t index) {
  table_view_->OnItemsRemoved(base::checked_cast<int>(index), 1);
  UpdateTableView();
}

void DeviceChooserContentView::OnOptionUpdated(size_t index) {
  table_view_->OnItemsChanged(base::checked_cast<int>(index), 1);
  UpdateTableView();
}

void DeviceChooserContentView::OnAdapterEnabledChanged(bool enabled) {
  // No row is selected since the adapter status has changed.
  // This will also disable the OK button if it was enabled because
  // of a previously selected row.
  table_view_->Select(-1);
  adapter_enabled_ = enabled;
  UpdateTableView();

  bluetooth_status_container_->ShowReScanButton(enabled);

  if (GetWidget() && GetWidget()->GetRootView())
    GetWidget()->GetRootView()->Layout();
}

void DeviceChooserContentView::OnRefreshStateChanged(bool refreshing) {
  if (refreshing) {
    // No row is selected since the chooser is refreshing. This will also
    // disable the OK button if it was enabled because of a previously
    // selected row.
    table_view_->Select(-1);
    UpdateTableView();
  }

  if (refreshing)
    bluetooth_status_container_->ShowScanningLabelAndThrobber();
  else
    bluetooth_status_container_->ShowReScanButton(true /* enabled */);

  if (GetWidget() && GetWidget()->GetRootView())
    GetWidget()->GetRootView()->Layout();
}

void DeviceChooserContentView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                      const gfx::Range& range,
                                                      int event_flags) {
  chooser_controller_->OpenAdapterOffHelpUrl();
}

void DeviceChooserContentView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  if (sender->tag() == kHelpButtonTag) {
    chooser_controller_->OpenHelpCenterUrl();
  } else {
    DCHECK_EQ(kReScanButtonTag, sender->tag());
    // Refreshing will cause the table view to yield focus, which
    // will land on the help button. Instead, briefly let the
    // rescan button take focus. When it hides itself, focus will
    // advance to the "Cancel" button as desired.
    sender->RequestFocus();
    chooser_controller_->RefreshOptions();
  }
}

base::string16 DeviceChooserContentView::GetWindowTitle() const {
  return chooser_controller_->GetTitle();
}

std::unique_ptr<views::View> DeviceChooserContentView::CreateExtraView() {
  const auto make_help_button = [this]() {
    auto help_button = views::CreateVectorImageButton(this);
    views::SetImageFromVectorIcon(help_button.get(),
                                  vector_icons::kHelpOutlineIcon);
    help_button->SetFocusForPlatform();
    help_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
    help_button->set_tag(kHelpButtonTag);
    return help_button;
  };

  const auto make_bluetooth_status_container = [this]() {
    auto bluetooth_status_container =
        std::make_unique<BluetoothStatusContainer>(this);
    bluetooth_status_container_ = bluetooth_status_container.get();
    return bluetooth_status_container;
  };

  const bool add_bluetooth = chooser_controller_->ShouldShowReScanButton();
  if (!chooser_controller_->ShouldShowHelpButton())
    return add_bluetooth ? make_bluetooth_status_container() : nullptr;
  if (!add_bluetooth)
    return make_help_button();

  auto container = std::make_unique<views::View>();
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  container->SetLayoutManager(std::move(layout))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  container->AddChildView(make_help_button());
  container->AddChildView(make_bluetooth_status_container());
  return container;
}

bool DeviceChooserContentView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return chooser_controller_->BothButtonsAlwaysEnabled() ||
         button != ui::DIALOG_BUTTON_OK ||
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
  bool has_options = adapter_enabled_ && RowCount() > 0;
  table_parent_->SetVisible(has_options);
  table_view_->SetEnabled(has_options &&
                          !chooser_controller_->TableViewAlwaysDisabled());
  no_options_view_->SetVisible(!has_options && adapter_enabled_);
  adapter_off_view_->SetVisible(!adapter_enabled_);
}

views::LabelButton* DeviceChooserContentView::ReScanButtonForTesting() {
  return bluetooth_status_container_->re_scan_button_;
}

views::Throbber* DeviceChooserContentView::ThrobberForTesting() {
  return bluetooth_status_container_->throbber_;
}

views::Label* DeviceChooserContentView::ScanningLabelForTesting() {
  return bluetooth_status_container_->scanning_label_;
}
