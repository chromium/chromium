// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_READ_LATER_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_READ_LATER_TOOLBAR_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/webui/read_later/read_later_ui.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"

class Browser;

namespace views {
class DotIndicator;
}  // namespace views

class ReadLaterToolbarButton : public ToolbarButton,
                               public ReadingListModelObserver {
 public:
  explicit ReadLaterToolbarButton(Browser* browser);
  ReadLaterToolbarButton(const ReadLaterToolbarButton&) = delete;
  ReadLaterToolbarButton& operator=(const ReadLaterToolbarButton&) = delete;
  ~ReadLaterToolbarButton() override;

  // ToolbarButton
  bool ShouldShowInkdropAfterIphInteraction() override;

  // Hides the Read Later side panel if showing, and updates the toolbar button
  // accordingly. Can be called to force close the side panel outside of a
  // toolbar button press (e.g. if the Lens side panel becomes active).
  // TODO(crbug.com/3130644): Remove this method and instead have the toolbar
  // button listen for side panel state changes.
  void HideSidePanel();

 private:
  class DotBoundsUpdater : public views::ViewObserver {
   public:
    DotBoundsUpdater(views::DotIndicator* dot_indicator,
                     views::ImageView* image);
    DotBoundsUpdater(const DotBoundsUpdater&) = delete;
    DotBoundsUpdater& operator=(const DotBoundsUpdater&) = delete;
    ~DotBoundsUpdater() override;

    // views::ViewObserver:
    void OnViewBoundsChanged(views::View* observed_view) override;

   private:
    const raw_ptr<views::DotIndicator> dot_indicator_;
    const raw_ptr<views::ImageView> image_;
    base::ScopedObservation<views::View, views::ViewObserver> observation_{
        this};
  };

  // ReadingListModelObserver:
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override;
  void ReadingListDidApplyChanges(ReadingListModel* model) override;

  void ButtonPressed();

  const raw_ptr<Browser> browser_;

  raw_ptr<views::DotIndicator> dot_indicator_;
  std::unique_ptr<DotBoundsUpdater> dot_bounds_updater_;

  const raw_ptr<ReadingListModel> reading_list_model_;
  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      reading_list_model_scoped_observation_{this};

  raw_ptr<views::View> side_panel_webview_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_READ_LATER_TOOLBAR_BUTTON_H_
