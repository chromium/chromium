// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_delegated_source_list_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"

namespace {
// Flag to display an informational message about using the system's
// screen-sharing picker. When disabled, only the button to open the picker is
// shown without further instructions.
BASE_FEATURE(kDelegatedSourceListInfoText,
             "DelegatedSourceListInfoText",
             base::FEATURE_ENABLED_BY_DEFAULT);

std::u16string GetMessageText(DesktopMediaList::Type type) {
  switch (type) {
    case DesktopMediaList::Type::kScreen:
      return l10n_util::GetStringUTF16(
          IDS_DESKTOP_MEDIA_PICKER_DELEGATED_SOURCE_LIST_SCREEN_TEXT);
    case DesktopMediaList::Type::kWindow:
      return l10n_util::GetStringUTF16(
          IDS_DESKTOP_MEDIA_PICKER_DELEGATED_SOURCE_LIST_WINDOW_TEXT);
    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
    case DesktopMediaList::Type::kNone:
      break;
  }
  NOTREACHED();
}

std::u16string GetButtonText(DesktopMediaList::Type type) {
  switch (type) {
    case DesktopMediaList::Type::kScreen:
      return l10n_util::GetStringUTF16(
          IDS_DESKTOP_MEDIA_PICKER_DELEGATED_SOURCE_LIST_SCREEN_BUTTON);
    case DesktopMediaList::Type::kWindow:
      return l10n_util::GetStringUTF16(
          IDS_DESKTOP_MEDIA_PICKER_DELEGATED_SOURCE_LIST_WINDOW_BUTTON);
    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
    case DesktopMediaList::Type::kNone:
      break;
  }
  NOTREACHED();
}

}  // namespace

DesktopMediaDelegatedSourceListView::DesktopMediaDelegatedSourceListView(
    base::WeakPtr<DesktopMediaListController> controller,
    const std::u16string& accessible_name,
    DesktopMediaList::Type type)
    : controller_(controller) {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(0),
          ChromeLayoutProvider::Get()->GetDistanceMetric(
              DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE)));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  if (base::FeatureList::IsEnabled(kDelegatedSourceListInfoText)) {
    label_ = AddChildView(std::make_unique<views::Label>());
    label_->SetText(GetMessageText(type));
  }
  button_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&DesktopMediaListController::ShowDelegatedList,
                          controller->GetWeakPtr())));
  button_->SetText(GetButtonText(type));
  button_->SetStyle(ui::ButtonStyle::kProminent);

  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(accessible_name);
}

DesktopMediaDelegatedSourceListView::~DesktopMediaDelegatedSourceListView() =
    default;

void DesktopMediaDelegatedSourceListView::OnSelectionChanged() {
  if (controller_) {
    controller_->OnSourceSelectionChanged();
  }
}

std::optional<content::DesktopMediaID>
DesktopMediaDelegatedSourceListView::GetSelection() {
  return selected_id_;
}

DesktopMediaListController::SourceListListener*
DesktopMediaDelegatedSourceListView::GetSourceListListener() {
  return this;
}

void DesktopMediaDelegatedSourceListView::ClearSelection() {
  selected_id_ = std::nullopt;
}

void DesktopMediaDelegatedSourceListView::OnSourceAdded(size_t index) {
  if (controller_) {
    selected_id_ = controller_->GetSource(index).id;
    controller_->OnSourceSelectionChanged();
  }
}

void DesktopMediaDelegatedSourceListView::OnSourceRemoved(size_t index) {
  if (selected_id_) {
    selected_id_ = std::nullopt;
    OnSelectionChanged();
  }
}

void DesktopMediaDelegatedSourceListView::OnSourceMoved(size_t old_index,
                                                        size_t new_index) {
  NOTREACHED();
}

void DesktopMediaDelegatedSourceListView::OnSourceNameChanged(size_t index) {
  NOTREACHED();
}

void DesktopMediaDelegatedSourceListView::OnSourceThumbnailChanged(
    size_t index) {
  NOTREACHED();
}

void DesktopMediaDelegatedSourceListView::OnSourcePreviewChanged(size_t index) {
  NOTREACHED();
}

void DesktopMediaDelegatedSourceListView::OnDelegatedSourceListSelection() {}
