// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_VIEW_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class BrowserWindowInterface;

// Holds the organizer panel and clips it during animations.
//
// Specific panels are created for normal organizer and other prototype
// surfaces.
class OrganizerPanelView : public views::View {
  METADATA_HEADER(OrganizerPanelView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kWebViewElementId);

  // Creates an appropriate panel view for the current browser configuration.
  // This is typically the "normal" Organizer view.
  static std::unique_ptr<OrganizerPanelView> Create(
      BrowserWindowInterface& browser);

  ~OrganizerPanelView() override;

  virtual bool IsInExtensionModeForTesting() const;

 protected:
  explicit OrganizerPanelView(BrowserWindowInterface& browser);

  // views::View:
  void Layout(PassKey) override;

 private:
  // Invalidates the view hierarchy when the panel animates.
  const base::CallbackListSubscription animation_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_VIEW_H_
