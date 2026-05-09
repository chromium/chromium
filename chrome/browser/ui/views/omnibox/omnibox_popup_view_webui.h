// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_WEBUI_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_WEBUI_H_

#include <stddef.h>

#include "base/gtest_prod_util.h"
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

class LocationBar;
class OmniboxController;
class OmniboxView;
class OmniboxPopupPresenterBase;
class OmniboxPopupPresenterDelegate;

class OmniboxPopupViewWebUI : public OmniboxPopupView,
                              OmniboxEditModel::Observer {
 public:
  OmniboxPopupViewWebUI(OmniboxView* omnibox_view,
                        OmniboxController* controller,
                        LocationBar* location_bar,
                        OmniboxPopupPresenterDelegate& presenter_delegate);
  OmniboxPopupViewWebUI(const OmniboxPopupViewWebUI&) = delete;
  OmniboxPopupViewWebUI& operator=(const OmniboxPopupViewWebUI&) = delete;
  ~OmniboxPopupViewWebUI() override;

  raw_ptr<OmniboxPopupPresenterBase> presenter() { return presenter_.get(); }

  // OmniboxPopupView:
  void InvalidateLine(size_t line) override;
  void UpdatePopupAppearance() override;
  void ProvideButtonFocusHint(size_t line) override;
  void OnDragCanceled() override;
  void GetPopupAccessibleNodeData(ui::AXNodeData* node_data) const override;
  void StepSelection(OmniboxPopupSelection::Direction direction,
                     OmniboxPopupSelection::Step step) override;
  void OpenCurrentSelection(WindowOpenDisposition disposition) override;
  bool IsSelectionPopupControlled() const override;

  // OmniboxEditModel::Observer:
  void OnSelectionChanged(OmniboxPopupSelection old_selection,
                          OmniboxPopupSelection selection) override {}
  void OnMatchIconUpdated(size_t index) override {}
  void OnContentsChanged() override;
  void OnKeywordStateChanged(bool is_keyword_selected) override {}
  void OnCharTyped(base::TimeTicks timestamp) override {}

 protected:
  // OmniboxPopupView:
  bool IsOpen() const override;

 private:
  // Time when this instance was constructed, or null after use for histogram.
  base::TimeTicks construction_time_;

  // The edit view owned by `location_bar_`. May be nullptr in tests.
  raw_ptr<OmniboxView> omnibox_view_;

  // The location bar that owns `this`. May be nullptr in tests.
  raw_ptr<LocationBar> location_bar_;

  // The presenter that manages its own widget and WebUI presentation.
  std::unique_ptr<OmniboxPopupPresenterBase> presenter_;

  // Observe `OmniboxEditModel` for updates that require updating the views.
  base::ScopedObservation<OmniboxEditModel, OmniboxEditModel::Observer>
      edit_model_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_WEBUI_H_
