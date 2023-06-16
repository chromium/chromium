// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_WEBUI_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_WEBUI_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

class LocationBarView;
class OmniboxEditModel;
class OmniboxViewViews;
class WebUIOmniboxPopupView;

class OmniboxPopupViewWebUI : public OmniboxPopupViewViews {
 public:
  OmniboxPopupViewWebUI(OmniboxViewViews* omnibox_view,
                        OmniboxEditModel* edit_model,
                        LocationBarView* location_bar_view);
  explicit OmniboxPopupViewWebUI(const OmniboxPopupViewViews&) = delete;
  OmniboxPopupViewWebUI& operator=(const OmniboxPopupViewViews&) = delete;

  // OmniboxPopupView:
  void OnSelectionChanged(OmniboxPopupSelection old_selection,
                          OmniboxPopupSelection new_selection) override;
  void ProvideButtonFocusHint(size_t line) override;
  void OnMatchIconUpdated(size_t match_index) override;
  void AddPopupAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::View:
  bool OnMouseDragged(const ui::MouseEvent& event) override;

 protected:
  friend class OmniboxPopupViewWebUITest;
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupViewWebUITest,
                           TestSatisfiesTestCoverageRobot);

  // OmniboxPopupViewViews:
  void UpdateChildViews() override;
  void OnPopupCreated() override;
  gfx::Rect GetTargetBounds() const override;

 private:
  // The reference to the child suggestions WebView.
  raw_ptr<WebUIOmniboxPopupView> webui_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_WEBUI_H_
