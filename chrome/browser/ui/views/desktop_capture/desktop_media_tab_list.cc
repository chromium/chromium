// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_tab_list.h"

#include "base/numerics/ranges.h"
#include "base/numerics/safe_conversions.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace {

// ui::TableModel that wraps a DesktopMediaListController and listens for
// updates from it.
class TabListModel : public ui::TableModel,
                     public DesktopMediaListController::SourceListListener {
 public:
  explicit TabListModel(DesktopMediaListController* controller);

  // ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int row, int column) override;
  gfx::ImageSkia GetIcon(int row) override;
  void SetObserver(ui::TableModelObserver* observer) override;

  // DesktopMediaListController::SourceListListener:
  void OnSourceAdded(size_t index) override;
  void OnSourceRemoved(size_t index) override;
  void OnSourceMoved(size_t old_index, size_t new_index) override;
  void OnSourceNameChanged(size_t index) override;
  void OnSourceThumbnailChanged(size_t index) override;

 private:
  TabListModel(const TabListModel&) = delete;
  TabListModel operator=(const TabListModel&) = delete;

  DesktopMediaListController* controller_;
  ui::TableModelObserver* observer_ = nullptr;
};

TabListModel::TabListModel(DesktopMediaListController* controller)
    : controller_(controller) {}

int TabListModel::RowCount() {
  return base::checked_cast<int>(controller_->GetSourceCount());
}

base::string16 TabListModel::GetText(int row, int column) {
  return controller_->GetSource(row).name;
}

gfx::ImageSkia TabListModel::GetIcon(int row) {
  return controller_->GetSource(row).thumbnail;
}

void TabListModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

void TabListModel::OnSourceAdded(size_t index) {
  observer_->OnItemsAdded(index, 1);
}

void TabListModel::OnSourceRemoved(size_t index) {
  observer_->OnItemsRemoved(index, 1);
}

void TabListModel::OnSourceMoved(size_t old_index, size_t new_index) {
  observer_->OnItemsMoved(old_index, 1, new_index);
}

void TabListModel::OnSourceNameChanged(size_t index) {
  observer_->OnItemsChanged(index, 1);
}

void TabListModel::OnSourceThumbnailChanged(size_t index) {
  observer_->OnItemsChanged(index, 1);
}

// TableViewObserver implementation that bridges between the actual TableView
// listing tabs and the DesktopMediaTabList.
class TabListViewObserver : public views::TableViewObserver {
 public:
  explicit TabListViewObserver(DesktopMediaListController* controller);

  void OnSelectionChanged() override;
  void OnDoubleClick() override;
  void OnKeyDown(ui::KeyboardCode virtual_keycode) override;

 private:
  TabListViewObserver(const TabListViewObserver&) = delete;
  TabListViewObserver operator=(const TabListViewObserver&) = delete;

  DesktopMediaListController* controller_;
};

TabListViewObserver::TabListViewObserver(DesktopMediaListController* controller)
    : controller_(controller) {}

void TabListViewObserver::OnSelectionChanged() {
  controller_->OnSourceSelectionChanged();
}

void TabListViewObserver::OnDoubleClick() {
  controller_->AcceptSource();
}

void TabListViewObserver::OnKeyDown(ui::KeyboardCode virtual_keycode) {
  if (virtual_keycode == ui::VKEY_RETURN)
    controller_->AcceptSource();
}

}  // namespace

DesktopMediaTabList::DesktopMediaTabList(DesktopMediaListController* controller,
                                         const base::string16& accessible_name)
    : controller_(controller) {
  // The thumbnail size isn't allowed to be smaller than gfx::kFaviconSize by
  // the underlying media list. TableView requires that the icon size be exactly
  // ui::TableModel::kIconSize; if it's not, rendering of the TableView breaks.
  // This DCHECK enforces that kIconSize is an acceptable size for the source
  // list.
  DCHECK_GE(ui::TableModel::kIconSize, gfx::kFaviconSize);

  controller_->SetThumbnailSize(
      gfx::Size(ui::TableModel::kIconSize, ui::TableModel::kIconSize));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  model_ = std::make_unique<TabListModel>(controller_);
  view_observer_ = std::make_unique<TabListViewObserver>(controller_);

  auto child = std::make_unique<views::TableView>(
      model_.get(), std::vector<ui::TableColumn>(1), views::ICON_AND_TEXT,
      true);
  child->set_observer(view_observer_.get());
  child->GetViewAccessibility().OverrideName(accessible_name);
  child->SetBorder(views::CreateSolidBorder(1, SK_ColorBLACK));
  child_ = child.get();

  AddChildView(views::TableView::CreateScrollViewWithTable(std::move(child)));
}

DesktopMediaTabList::~DesktopMediaTabList() {
  child_->SetModel(nullptr);
}

const char* DesktopMediaTabList::GetClassName() const {
  return "DesktopMediaTabList";
}

gfx::Size DesktopMediaTabList::CalculatePreferredSize() const {
  // The picker should have a fixed height of 10 rows.
  return gfx::Size(0, child_->GetRowHeight() * 10);
}

int DesktopMediaTabList::GetHeightForWidth(int width) const {
  // If this method isn't overridden here, the default implementation would fall
  // back to FillLayout's GetHeightForWidth, which would ask the TableView,
  // which would return something based on the total number of rows, since
  // TableView expects to always be sized by its container. Avoid even asking it
  // by using the same height as CalculatePreferredSize().
  return CalculatePreferredSize().height();
}

base::Optional<content::DesktopMediaID> DesktopMediaTabList::GetSelection() {
  int row = child_->GetFirstSelectedRow();
  if (row == -1)
    return base::nullopt;
  return controller_->GetSource(row).id;
}

DesktopMediaListController::SourceListListener*
DesktopMediaTabList::GetSourceListListener() {
  return model_.get();
}
