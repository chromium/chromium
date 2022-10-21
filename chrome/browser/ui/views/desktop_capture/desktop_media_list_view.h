// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_LIST_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"
#include "content/public/browser/desktop_media_id.h"
#include "ui/views/view.h"

class DesktopMediaListController;

// View that shows a list of desktop media sources available from
// DesktopMediaList.
class DesktopMediaListView
    : public DesktopMediaListController::ListView,
      public DesktopMediaListController::SourceListListener {
 public:
  DesktopMediaListView(DesktopMediaListController* controller,
                       DesktopMediaSourceViewStyle generic_style,
                       DesktopMediaSourceViewStyle single_style,
                       const std::u16string& accessible_name);

  DesktopMediaListView(const DesktopMediaListView&) = delete;
  DesktopMediaListView& operator=(const DesktopMediaListView&) = delete;

  ~DesktopMediaListView() override;

  // Called by DesktopMediaSourceView when selection has changed.
  void OnSelectionChanged();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // DesktopMediaListController::ListView:
  absl::optional<content::DesktopMediaID> GetSelection() override;
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
  // Change the source style of this list on the fly.
  void SetStyle(DesktopMediaSourceViewStyle* style);

  DesktopMediaSourceView* GetSelectedView();

  raw_ptr<DesktopMediaListController, DanglingUntriaged> controller_;

  DesktopMediaSourceViewStyle single_style_;
  DesktopMediaSourceViewStyle generic_style_;
  raw_ptr<DesktopMediaSourceViewStyle, DanglingUntriaged> active_style_;

  const std::u16string accessible_name_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_LIST_VIEW_H_
