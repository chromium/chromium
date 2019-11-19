// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"

#include "base/command_line.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_tab_list.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

DesktopMediaListController::DesktopMediaListController(
    DesktopMediaPickerDialogView* parent,
    std::unique_ptr<DesktopMediaList> media_list)
    : dialog_(parent), media_list_(std::move(media_list)) {}

DesktopMediaListController::~DesktopMediaListController() = default;

std::unique_ptr<views::View> DesktopMediaListController::CreateView(
    DesktopMediaSourceViewStyle generic_style,
    DesktopMediaSourceViewStyle single_style,
    const base::string16& accessible_name) {
  DCHECK(!view_);

  auto view = std::make_unique<DesktopMediaListView>(
      this, generic_style, single_style, accessible_name);
  view_ = view.get();
  view_observer_.Add(view_);
  return view;
}

std::unique_ptr<views::View> DesktopMediaListController::CreateTabListView(
    const base::string16& accessible_name) {
  DCHECK(!view_);

  auto view = std::make_unique<DesktopMediaTabList>(this, accessible_name);
  view_ = view.get();
  view_observer_.Add(view_);
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
  dialog_->AcceptSource();
}

void DesktopMediaListController::AcceptSpecificSource(
    content::DesktopMediaID source) {
  dialog_->AcceptSpecificSource(source);
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

  std::string autoselect_source =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAutoSelectDesktopCaptureSource);
  const DesktopMediaList::Source& source = GetSource(index);
  if (autoselect_source.empty() ||
      base::ASCIIToUTF16(autoselect_source) != source.name) {
    return;
  }
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&DesktopMediaListController::AcceptSpecificSource,
                     weak_factory_.GetWeakPtr(), source.id));
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
  view_observer_.Remove(view);
  view_ = nullptr;
}
