// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_SIDE_PANEL_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_SIDE_PANEL_TOOLBAR_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_ui.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "ui/views/controls/dot_indicator.h"

class Browser;

class SidePanelToolbarButton : public ToolbarButton,
                               public ReadingListModelObserver {
 public:
  explicit SidePanelToolbarButton(Browser* browser);
  SidePanelToolbarButton(const SidePanelToolbarButton&) = delete;
  SidePanelToolbarButton& operator=(const SidePanelToolbarButton&) = delete;
  ~SidePanelToolbarButton() override;

  // ToolbarButton
  bool ShouldShowInkdropAfterIphInteraction() override;

  // Hides the Read Later side panel if showing, and updates the toolbar button
  // accordingly. Can be called to force close the side panel outside of a
  // toolbar button press (e.g. if the Lens side panel becomes active).
  // TODO(crbug.com/3130644): Remove this method and instead have the toolbar
  // button listen for side panel state changes.
  void HideSidePanel();

  bool GetDotIndicatorVisibilityForTesting() const {
    return dot_indicator_->GetVisible();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(SidePanelToolbarButtonTest, SetCorrectIconInLTR);
  FRIEND_TEST_ALL_PREFIXES(SidePanelToolbarButtonTest, SetCorrectIconInRTL);

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

  // Updates the vector icon used when the PrefChangeRegistrar listens to a
  // change. When the side panel should open to the right side of the browser
  // the default vector icon is used. When the side panel should open to the
  // left side of the browser the flipped vector icon is used.
  void UpdateToolbarButtonIcon();

  const raw_ptr<Browser> browser_;

  raw_ptr<views::DotIndicator> dot_indicator_;
  std::unique_ptr<DotBoundsUpdater> dot_bounds_updater_;

  const raw_ptr<ReadingListModel> reading_list_model_;
  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      reading_list_model_scoped_observation_{this};

  raw_ptr<views::View, DanglingUntriaged> side_panel_webview_ = nullptr;

  // Observes and listens to side panel alignment changes.
  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_SIDE_PANEL_TOOLBAR_BUTTON_H_
