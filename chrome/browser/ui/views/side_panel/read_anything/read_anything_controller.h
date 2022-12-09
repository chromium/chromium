// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_font_combobox.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toolbar_view.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_page_handler.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_update_forward.h"
#include "ui/base/models/combobox_model.h"

class Browser;

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingController
//
//  A class that controls the Read Anything feature. This class does all of the
//  business logic of this feature and updates the model.
//  The controller is meant to be internal to the Read Anything feature and
//  classes outside this feature should not be making calls to it. The
//  coordinator is the external-facing API.
//  This class is owned by the ReadAnythingCoordinator and has the same lifetime
//  as the browser.
//
class ReadAnythingController : public ReadAnythingToolbarView::Delegate,
                               public ReadAnythingFontCombobox::Delegate,
                               public ReadAnythingPageHandler::Delegate,
                               public TabStripModelObserver,
                               public content::WebContentsObserver {
 public:
  ReadAnythingController(ReadAnythingModel* model, Browser* browser);
  ReadAnythingController(const ReadAnythingController&) = delete;
  ReadAnythingController& operator=(const ReadAnythingController&) = delete;
  ~ReadAnythingController() override;

  // Called to activate or de-activate Read Anything. The feature is active when
  // it is currently shown in the side panel.
  void Activate(bool active);
  bool IsActiveForTesting() { return active_; }

 private:
  friend class ReadAnythingControllerTest;

  // ReadAnythingFontCombobox::Delegate:
  void OnFontChoiceChanged(int new_index) override;
  ui::ComboboxModel* GetFontComboboxModel() override;

  // ReadAnythingToolbarView::Delegate:
  void OnFontSizeChanged(bool increase) override;
  void OnColorsChanged(int new_index) override;
  ReadAnythingMenuModel* GetColorsModel() override;
  void OnLineSpacingChanged(int new_index) override;
  ReadAnythingMenuModel* GetLineSpacingModel() override;
  void OnLetterSpacingChanged(int new_index) override;
  ReadAnythingMenuModel* GetLetterSpacingModel() override;

  // ReadAnythingPageHandler::Delegate:
  void OnUIReady() override;
  void OnUIDestroyed() override;
  void OnLinkClicked(const GURL& url, bool open_in_new_tab) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabStripModelDestroyed(TabStripModel* tab_strip_model) override;

  // content::WebContentsObserver:
  void DidStopLoading() override;

  // Requests a distilled AXTree for the main frame of the currently active
  // web contents.
  void DistillAXTree();

  // Callback method which receives an AXTree snapshot and a list of AXNodes
  // which correspond to nodes in the tree that contain main content.
  void OnAXTreeDistilled(const ui::AXTreeUpdate& snapshot,
                         const std::vector<ui::AXNodeID>& content_node_ids);

  const raw_ptr<ReadAnythingModel> model_;
  std::vector<ReadAnythingModel::Observer*> observers_;

  // ReadAnythingController is owned by ReadAnythingCoordinator which is a
  // browser user data, so this pointer is always valid.
  raw_ptr<Browser> browser_;

  // Whether the Read Anything feature is currently active. The feature is
  // active when it is currently shown in the Side Panel.
  bool active_ = false;

  // Whether the Read Anything feature's UI is ready. This is set to true when
  // the UI is constructed and false when it is destroyed.
  bool ui_ready_ = false;

  base::WeakPtrFactory<ReadAnythingController> weak_pointer_factory_{this};
};
#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONTROLLER_H_
