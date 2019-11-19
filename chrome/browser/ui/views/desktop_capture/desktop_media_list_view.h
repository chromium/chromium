// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_LIST_VIEW_H_

#include <memory>

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
                       const base::string16& accessible_name);

  ~DesktopMediaListView() override;

  // Called by DesktopMediaSourceView when selection has changed.
  void OnSelectionChanged();

  // Called by DesktopMediaSourceView when a source has been double-clicked.
  void OnDoubleClick();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // DesktopMediaListController::ListView:
  base::Optional<content::DesktopMediaID> GetSelection() override;
  DesktopMediaListController::SourceListListener* GetSourceListListener()
      override;

  // DesktopMediaListController::SourceListListener:
  void OnSourceAdded(size_t index) override;
  void OnSourceRemoved(size_t index) override;
  void OnSourceMoved(size_t old_index, size_t new_index) override;
  void OnSourceNameChanged(size_t index) override;
  void OnSourceThumbnailChanged(size_t index) override;

 private:
  // Change the source style of this list on the fly.
  void SetStyle(DesktopMediaSourceViewStyle* style);

  DesktopMediaSourceView* GetSelectedView();

  DesktopMediaListController* controller_;

  DesktopMediaSourceViewStyle single_style_;
  DesktopMediaSourceViewStyle generic_style_;
  DesktopMediaSourceViewStyle* active_style_;

  const base::string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(DesktopMediaListView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_LIST_VIEW_H_
