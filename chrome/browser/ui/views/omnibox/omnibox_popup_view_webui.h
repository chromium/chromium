// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_WEBUI_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_WEBUI_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
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
class OmniboxController;
class OmniboxViewViews;
class OmniboxPopupPresenter;

class OmniboxPopupViewWebUI : public OmniboxPopupView {
 public:
  OmniboxPopupViewWebUI(OmniboxViewViews* omnibox_view,
                        OmniboxController* controller,
                        LocationBarView* location_bar_view);
  OmniboxPopupViewWebUI(const OmniboxPopupViewWebUI&) = delete;
  OmniboxPopupViewWebUI& operator=(const OmniboxPopupViewWebUI&) = delete;
  ~OmniboxPopupViewWebUI() override;

  // OmniboxPopupView:
  bool IsOpen() const override;
  void InvalidateLine(size_t line) override;
  void OnSelectionChanged(OmniboxPopupSelection old_selection,
                          OmniboxPopupSelection new_selection) override;
  void UpdatePopupAppearance() override;
  void ProvideButtonFocusHint(size_t line) override;
  void OnMatchIconUpdated(size_t match_index) override;
  void OnDragCanceled() override;
  void GetPopupAccessibleNodeData(ui::AXNodeData* node_data) override;
  void AddPopupAccessibleNodeData(ui::AXNodeData* node_data) override;
  std::u16string GetAccessibleButtonTextForResult(size_t line) override;

 protected:
  friend class OmniboxPopupViewWebUITest;
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupViewWebUITest,
                           PopupLoadsAndAcceptsCalls);

 private:
  // Time when this instance was constructed, or null after use for histogram.
  base::TimeTicks construction_time_;

  // The edit view that invokes us. May be nullptr in tests.
  raw_ptr<OmniboxViewViews> omnibox_view_;

  // The location bar view that owns `omnibox_view_`. May be nullptr in tests.
  raw_ptr<LocationBarView> location_bar_view_;

  // The presenter that manages its own widget and WebUI presentation.
  std::unique_ptr<OmniboxPopupPresenter> presenter_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_VIEW_WEBUI_H_
