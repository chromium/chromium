// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_VIEW_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/infobars/core/infobar_container_with_priority.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/view_targeter_delegate.h"

class InfoBarView;

// The views-specific implementation of InfoBarContainer.
class InfoBarContainerView : public views::AccessiblePaneView,
                             public infobars::InfoBarContainerWithPriority {
  METADATA_HEADER(InfoBarContainerView, views::AccessiblePaneView)

 public:
  explicit InfoBarContainerView(Delegate* delegate);
  InfoBarContainerView(const InfoBarContainerView&) = delete;
  InfoBarContainerView& operator=(const InfoBarContainerView&) = delete;
  ~InfoBarContainerView() override;

  // Returns true if there are no infobars.
  bool IsEmpty() const;

  // views::AccessiblePaneView:
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // InfobarContainer:
  void ChangeInfoBarManager(infobars::InfoBarManager* infobar_manager) override;
  void PlatformSpecificAddInfoBar(infobars::InfoBar* infobar,
                                  size_t position) override;
  void PlatformSpecificRemoveInfoBar(infobars::InfoBar* infobar) override;
  void PlatformSpecificWillRemoveInfoBar(infobars::InfoBar* infobar) override;
  void PlatformSpecificInfoBarShown(infobars::InfoBar* infobar) override;
  void PlatformSpecificPendingInfoBarCountChanged(size_t count) override;

 protected:
  // infobars::InfoBarContainerWithPriority:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override;

 private:
  // Re-evaluates which infobar hosts the queued sequence indicator and its
  // "X of Y" position. The host is the bottom-most infobar that is staying,
  // unless it is kCriticalSecurity or non-closeable, which suppresses the
  // indicator. Safe to call redundantly. With `preserve_indicator_host`, moving
  // the indicator to a new host does not count as a new Shown.
  void UpdateQueuedIndicator(const infobars::InfoBar* removing = nullptr,
                             bool preserve_indicator_host = false);

  // This view draws the shadow over the web contents below the
  // lowest infobar. A separate view with a layer is used so it can
  // draw outside the bounds of |this|.
  raw_ptr<views::View> content_shadow_ = nullptr;

  // The infobar hosting the queued sequence indicator, or null. Cleared in
  // PlatformSpecificRemoveInfoBar before the host's view goes away.
  raw_ptr<InfoBarView> queued_indicator_host_ = nullptr;

  // When the indicator appeared on `queued_indicator_host_` with pending
  // infobars behind it. It feeds HostResolveTime, recorded in OnInfoBarRemoved
  // so tab switches never contribute samples. Null when no pending infobars
  // remain (e.g. on the final "3 of 3" infobar).
  base::TimeTicks indicator_shown_on_host_at_;

  // True during compound container mutations (e.g: manager swap,
  // removal/promotion, replacement), deferring indicator updates until state
  // has settled.
  bool defer_indicator_updates_ = false;

  // True when the focused view was inside this container at the moment an
  // infobar removal started. Used to restore focus back to the infobar region
  // when the next infobar is shown (e.g. a queued infobar being promoted), so
  // focus does not jump to page contents and screen reader users are made aware
  // of the newly surfaced infobar.
  bool restore_focus_on_next_shown_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_VIEW_H_
