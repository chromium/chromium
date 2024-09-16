// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_DELEGATED_SOURCE_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_DELEGATED_SOURCE_LIST_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"
#include "ui/views/controls/button/md_text_button.h"

class DesktopMediaListController;

// View for delegated source lists where no thumbnail previews are provided.
class DesktopMediaDelegatedSourceListView
    : public DesktopMediaListController::ListView,
      public DesktopMediaListController::SourceListListener {
 public:
  DesktopMediaDelegatedSourceListView(
      base::WeakPtr<DesktopMediaListController> controller,
      const std::u16string& accessible_name,
      DesktopMediaList::Type type);

  DesktopMediaDelegatedSourceListView(
      const DesktopMediaDelegatedSourceListView&) = delete;
  DesktopMediaDelegatedSourceListView& operator=(
      const DesktopMediaDelegatedSourceListView&) = delete;

  ~DesktopMediaDelegatedSourceListView() override;

  void OnSelectionChanged();

  // DesktopMediaListController::ListView:
  std::optional<content::DesktopMediaID> GetSelection() override;
  DesktopMediaListController::SourceListListener* GetSourceListListener()
      override;
  void ClearSelection() override;

  // DesktopMediaListController::SourceListListener:
  void OnSourceAdded(size_t index) override;
  void OnSourceRemoved(size_t index) override;
  void OnSourceMoved(size_t old_index, size_t new_index) override;
  void OnSourceNameChanged(size_t index) override;
  void OnSourceThumbnailChanged(size_t index) override;
  void OnSourcePreviewChanged(size_t index) override;
  void OnDelegatedSourceListSelection() override;

 private:
  base::WeakPtr<DesktopMediaListController> controller_;

  std::optional<content::DesktopMediaID> selected_id_;

  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::MdTextButton> button_ = nullptr;

  base::WeakPtrFactory<DesktopMediaDelegatedSourceListView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_DELEGATED_SOURCE_LIST_VIEW_H_
