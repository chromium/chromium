// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_WEBUI_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_WEBUI_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

class LocationBarView;
class OmniboxController;
class OmniboxViewViews;
class OmniboxPopupPresenter;

class OmniboxPopupViewWebUI : public OmniboxPopupView,
                              OmniboxEditModel::Observer {
 public:
  OmniboxPopupViewWebUI(OmniboxViewViews* omnibox_view,
                        OmniboxController* controller,
                        LocationBarView* location_bar_view);
  OmniboxPopupViewWebUI(const OmniboxPopupViewWebUI&) = delete;
  OmniboxPopupViewWebUI& operator=(const OmniboxPopupViewWebUI&) = delete;
  ~OmniboxPopupViewWebUI() override;

  raw_ptr<OmniboxPopupPresenter> presenter() { return presenter_.get(); }

  // OmniboxPopupView:
  void InvalidateLine(size_t line) override;
  void UpdatePopupAppearance() override;
  void ProvideButtonFocusHint(size_t line) override;
  void OnDragCanceled() override;
  void GetPopupAccessibleNodeData(ui::AXNodeData* node_data) const override;
  raw_ptr<OmniboxPopupViewWebUI> GetOmniboxPopupViewWebUI() override;

  // OmniboxEditModel::Observer:
  void OnSelectionChanged(OmniboxPopupSelection old_selection,
                          OmniboxPopupSelection selection) override {}
  void OnMatchIconUpdated(size_t index) override {}
  void OnContentsChanged() override;
  void OnKeywordStateChanged(bool is_keyword_selected) override {}

 protected:
  friend class OmniboxPopupViewWebUITest;
  friend class OmniboxWebUiInteractiveTest;
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupViewWebUITest,
                           PopupLoadsAndAcceptsCalls);

  // OmniboxPopupView:
  bool IsOpen() const override;

 private:
  // Time when this instance was constructed, or null after use for histogram.
  base::TimeTicks construction_time_;

  // The edit view owned by `location_bar_view_`. May be nullptr in tests.
  raw_ptr<OmniboxViewViews> omnibox_view_;

  // The location bar view that owns `this`. May be nullptr in tests.
  raw_ptr<LocationBarView> location_bar_view_;

  // The presenter that manages its own widget and WebUI presentation.
  std::unique_ptr<OmniboxPopupPresenter> presenter_;

  // Observe `OmniboxEditModel` for updates that require updating the views.
  base::ScopedObservation<OmniboxEditModel, OmniboxEditModel::Observer>
      edit_model_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_WEBUI_H_
