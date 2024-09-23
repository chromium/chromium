// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_TAB_LIST_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_TAB_LIST_ROW_VIEW_H_

#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/performance_controls/tab_list_model.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

class TextContainer;

namespace views {
class ImageButton;
class InkDropContainerView;
}  // namespace views

class TabListRowView : public views::View,
                       public views::FocusChangeListener,
                       public TabListModel::Observer {
  METADATA_HEADER(TabListRowView, views::View)
 public:
  TabListRowView(
      resource_attribution::PageContext tab,
      TabListModel* tab_list_model,
      base::OnceCallback<void(TabListRowView*)> close_button_callback);
  ~TabListRowView() override;

  TabListRowView(const TabListRowView&) = delete;
  TabListRowView& operator=(const TabListRowView&) = delete;

  std::u16string GetTitleTextForTesting();
  views::ImageButton* GetCloseButtonForTesting();
  views::View* GetTextContainerForTesting();

  // views::View override:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void AddLayerToRegion(ui::Layer* layer, views::LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* layer) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* before, views::View* now) override {}
  void OnDidChangeFocus(views::View* before, views::View* now) override;

  // TabListModel::Observer:
  void OnTabCountChanged(int count) override;

  void RefreshInkDropAndCloseButton();

 private:
  std::unique_ptr<views::View> CreateTextView(std::u16string title,
                                              GURL domain);

  // Focuses on the close button if we are currently focusing on a
  // non-descendant view of the tab list row.
  void MaybeFocusCloseButton();

  resource_attribution::PageContext actionable_tab_;
  raw_ptr<TabListModel> tab_list_model_ = nullptr;
  raw_ptr<views::ImageButton> close_button_ = nullptr;
  raw_ptr<TextContainer> text_container_ = nullptr;
  raw_ptr<views::InkDropContainerView> inkdrop_container_ = nullptr;
  base::ScopedObservation<TabListModel, TabListModel::Observer>
      tab_list_model_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_TAB_LIST_ROW_VIEW_H_
