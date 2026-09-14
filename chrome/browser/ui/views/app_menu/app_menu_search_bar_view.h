// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SEARCH_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SEARCH_BAR_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/events/event_observer.h"
#include "ui/views/controls/textfield/textfield.h"

namespace views {
class EventMonitor;
class ImageView;
}  // namespace views

// Search bar view placed at the top of the Block Style ChroMenu.
class AppMenuSearchBarView : public views::Textfield, public ui::EventObserver {
  METADATA_HEADER(AppMenuSearchBarView, views::Textfield)

 public:
  using views::Textfield::OnEvent;

  AppMenuSearchBarView();
  AppMenuSearchBarView(const AppMenuSearchBarView&) = delete;
  AppMenuSearchBarView& operator=(const AppMenuSearchBarView&) = delete;
  ~AppMenuSearchBarView() override;

  views::ImageView* search_icon_for_testing() { return search_icon_; }
  const views::ImageView* search_icon_for_testing() const {
    return search_icon_;
  }
  bool is_active_for_testing() const { return is_active_; }

  void HandleKeyEvent(ui::KeyEvent* event);

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

  // views::Textfield:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void Layout(PassKey) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;

 private:
  void SetTextfieldFocused(bool focused);

  bool is_active_ = false;
  raw_ptr<views::ImageView> search_icon_ = nullptr;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_SEARCH_BAR_VIEW_H_
