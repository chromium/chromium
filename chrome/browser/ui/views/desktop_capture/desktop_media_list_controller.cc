// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"

#include "base/command_line.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_tab_list.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/views/metadata/metadata_impl_macros.h"

BEGIN_METADATA(DesktopMediaListController, ListView, views::View)
END_METADATA

DesktopMediaListController::DesktopMediaListController(
    DesktopMediaPickerDialogView* parent,
    std::unique_ptr<DesktopMediaList> media_list)
    : dialog_(parent),
      media_list_(std::move(media_list)),
      auto_select_source_(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kAutoSelectDesktopCaptureSource)),
      auto_accept_tab_capture_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kThisTabCaptureAutoAccept)),
      auto_reject_tab_capture_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kThisTabCaptureAutoReject)) {}

DesktopMediaListController::~DesktopMediaListController() = default;

std::unique_ptr<views::View> DesktopMediaListController::CreateView(
    DesktopMediaSourceViewStyle generic_style,
    DesktopMediaSourceViewStyle single_style,
    const std::u16string& accessible_name) {
  DCHECK(!view_);

  auto view = std::make_unique<DesktopMediaListView>(
      this, generic_style, single_style, accessible_name);
  view_ = view.get();
  view_observations_.AddObservation(view_);
  return view;
}

std::unique_ptr<views::View> DesktopMediaListController::CreateTabListView(
    const std::u16string& accessible_name) {
  DCHECK(!view_);

  auto view = std::make_unique<DesktopMediaTabList>(this, accessible_name);
  view_ = view.get();
  view_observations_.AddObservation(view_);
  return view;
}

void DesktopMediaListController::StartUpdating(
    content::DesktopMediaID dialog_window_id) {
  media_list_->SetViewDialogWindowId(dialog_window_id);
  media_list_->StartUpdating(this);
}

void DesktopMediaListController::FocusView() {
  if (view_)
    view_->RequestFocus();
}

base::Optional<content::DesktopMediaID>
DesktopMediaListController::GetSelection() const {
  return view_ ? view_->GetSelection() : base::nullopt;
}

void DesktopMediaListController::OnSourceListLayoutChanged() {
  dialog_->OnSourceListLayoutChanged();
}

void DesktopMediaListController::OnSourceSelectionChanged() {
  dialog_->OnSelectionChanged();
}

void DesktopMediaListController::AcceptSource() {
  if (GetSelection())
    dialog_->AcceptSource();
}

void DesktopMediaListController::AcceptSpecificSource(
    content::DesktopMediaID source) {
  dialog_->AcceptSpecificSource(source);
}

void DesktopMediaListController::Reject() {
  dialog_->Reject();
}

size_t DesktopMediaListController::GetSourceCount() const {
  return base::checked_cast<size_t>(media_list_->GetSourceCount());
}

const DesktopMediaList::Source& DesktopMediaListController::GetSource(
    size_t index) const {
  return media_list_->GetSource(index);
}

void DesktopMediaListController::SetThumbnailSize(const gfx::Size& size) {
  media_list_->SetThumbnailSize(size);
}

void DesktopMediaListController::OnSourceAdded(DesktopMediaList* list,
                                               int index) {
  if (view_) {
    view_->GetSourceListListener()->OnSourceAdded(
        base::checked_cast<size_t>(index));
  }

  const DesktopMediaList::Source& source = GetSource(index);

  if (ShouldAutoAccept(source)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DesktopMediaListController::AcceptSpecificSource,
                       weak_factory_.GetWeakPtr(), source.id));
  } else if (ShouldAutoReject(source)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&DesktopMediaListController::Reject,
                                  weak_factory_.GetWeakPtr()));
  }
}

void DesktopMediaListController::OnSourceRemoved(DesktopMediaList* list,
                                                 int index) {
  if (view_) {
    view_->GetSourceListListener()->OnSourceRemoved(
        base::checked_cast<size_t>(index));
  }
}

void DesktopMediaListController::OnSourceMoved(DesktopMediaList* list,
                                               int old_index,
                                               int new_index) {
  if (view_) {
    view_->GetSourceListListener()->OnSourceMoved(
        base::checked_cast<size_t>(old_index),
        base::checked_cast<size_t>(new_index));
  }
}
void DesktopMediaListController::OnSourceNameChanged(DesktopMediaList* list,
                                                     int index) {
  if (view_) {
    view_->GetSourceListListener()->OnSourceNameChanged(
        base::checked_cast<size_t>(index));
  }
}
void DesktopMediaListController::OnSourceThumbnailChanged(
    DesktopMediaList* list,
    int index) {
  if (view_) {
    view_->GetSourceListListener()->OnSourceThumbnailChanged(
        base::checked_cast<size_t>(index));
  }
}

void DesktopMediaListController::OnViewIsDeleting(views::View* view) {
  view_observations_.RemoveObservation(view);
  view_ = nullptr;
}

bool DesktopMediaListController::ShouldAutoAccept(
    const DesktopMediaList::Source& source) const {
  if (media_list_->GetMediaListType() == DesktopMediaList::Type::kCurrentTab) {
    return auto_accept_tab_capture_;
  }

  return (!auto_select_source_.empty() &&
          source.name.find(base::ASCIIToUTF16(auto_select_source_)) !=
              std::u16string::npos);
}

bool DesktopMediaListController::ShouldAutoReject(
    const DesktopMediaList::Source& source) const {
  if (media_list_->GetMediaListType() == DesktopMediaList::Type::kCurrentTab) {
    return auto_reject_tab_capture_;
  }
  return false;
}
