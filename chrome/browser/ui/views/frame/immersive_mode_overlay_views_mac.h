// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_OVERLAY_VIEWS_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_OVERLAY_VIEWS_MAC_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/canvas.h"

class BrowserView;

// OverlayWidgetMac is a child Widget of BrowserWidget used during immersive
// fullscreen on macOS that hosts the top container. Its native Window and View
// interface with macOS fullscreen APIs allowing separation of the top container
// and web contents.
// Currently the only explicit reason for OverlayWidgetMac to be its own
// subclass is to support GetAccelerator() forwarding.
class OverlayWidgetMac : public ThemeCopyingWidget {
 public:
  ~OverlayWidgetMac() override;

  // Create an overlay widget for `browser_view`.
  static OverlayWidgetMac* Create(BrowserView* browser_view,
                                  views::Widget* parent);

  // ThemeCopyingWidget:

  // OverlayWidgetMac hosts the top container. Views within the top container
  // look up accelerators by asking their hosting Widget. In non-immersive
  // fullscreen that would be the BrowserWidget. Give top chrome what it expects
  // and forward GetAccelerator() calls to OverlayWidgetMac's parent
  // (BrowserWidget).
  bool GetAccelerator(int cmd_id, ui::Accelerator* accelerator) const override;

  // Instances of OverlayWidgetMac do not activate directly but their views
  // style should follow the parent (browser frame) activation state. In other
  // words, when the browser frame is not activate the overlay widget views will
  // appear disabled.
  bool ShouldViewsStyleFollowWidgetActivation() const override;

 private:
  explicit OverlayWidgetMac(views::Widget* role_model);
};

// TabContainerOverlayView is a view that hosts the TabStripRegionView during
// immersive fullscreen. The TopContainerView usually draws the background for
// the tab strip. Since the tab strip has been reparented we need to handle
// drawing the background here.
class TabContainerOverlayViewMac : public views::View {
  METADATA_HEADER(TabContainerOverlayViewMac, views::View)

 public:
  explicit TabContainerOverlayViewMac(base::WeakPtr<BrowserView> browser_view);
  ~TabContainerOverlayViewMac() override;

  // views::View:

  void OnPaintBackground(gfx::Canvas* canvas) override;

  // `BrowserRootView` handles drag and drop for the tab strip. In immersive
  // fullscreen, the tab strip is hosted in a separate Widget, in a separate
  // view, this view` TabContainerOverlayView`. To support drag and drop for the
  // tab strip in immersive fullscreen, forward all drag and drop requests to
  // the `BrowserRootView`.

  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  DropCallback GetDropCallback(const ui::DropTargetEvent& event) override;

 private:
  // The BrowserView this overlay is created for. WeakPtr is used since
  // this view is held in a different hierarchy.
  base::WeakPtr<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_OVERLAY_VIEWS_MAC_H_
