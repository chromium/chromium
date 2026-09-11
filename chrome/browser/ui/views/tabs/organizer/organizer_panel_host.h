// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_HOST_H_

#include <memory>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class BrowserWindowInterface;

// Object that hosts an organizer panel view.
class OrganizerPanelHost {
 public:
  OrganizerPanelHost() = default;
  OrganizerPanelHost(const OrganizerPanelHost&) = delete;
  void operator=(const OrganizerPanelHost&) = delete;
  virtual ~OrganizerPanelHost() = default;

  virtual void SetPanelView(std::unique_ptr<views::View> panel_view) = 0;
  virtual std::unique_ptr<views::View> TakePanelView() = 0;
  virtual bool HasPanelView() const = 0;

  // Returns the host interface if `view` or one of its ancestors is a known
  // implementation, otherwise null.
  static OrganizerPanelHost* FromView(views::View* view);

  // Returns the preferred panel host for the current state of `browser`.
  static OrganizerPanelHost* GetPreferredHost(BrowserWindowInterface& browser);
};

// Abstract View which implements `OrganizerPanelHost`. Not every implementation
// of OrganizerPanelHots can be derived from this view, but it does make tests
// with fake panel hosts for tests easier to implement.
class OrganizerPanelHostView : public views::View, public OrganizerPanelHost {
  METADATA_HEADER(OrganizerPanelHostView, views::View)
 public:
  OrganizerPanelHostView();
  ~OrganizerPanelHostView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_HOST_H_
