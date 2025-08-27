// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_INFOBAR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "chrome/browser/ui/views/tab_sharing/tab_sharing_status_message_view.h"

namespace views {
class MdTextButton;
}  // namespace views

// The infobar displayed when sharing a tab. It shows:
// - a message informing the user about which site is shared with which site
// - an optional button for quick navigation to the capturing/captured tab
// - an optional button for sharing the currently displayed tab instead
// - a button to stop the capture
class TabSharingInfoBar : public InfoBarView {
 public:
  TabSharingInfoBar(
      std::unique_ptr<TabSharingInfoBarDelegate> delegate,
      content::GlobalRenderFrameHostId shared_tab_id,
      content::GlobalRenderFrameHostId capturer_id,
      const std::u16string& shared_tab_name,
      const std::u16string& capturer_name,
      TabSharingInfoBarDelegate::TabRole role,
      TabSharingInfoBarDelegate::TabShareType capture_type,
      base::WeakPtr<ScreensharingControlsHistogramLogger> uma_logger);

  TabSharingInfoBar(const TabSharingInfoBar&) = delete;
  TabSharingInfoBar& operator=(const TabSharingInfoBar&) = delete;

  ~TabSharingInfoBar() override;

  // InfoBarView:
  void Layout(PassKey) override;

  const views::View* GetStatusMessageViewForTesting() const {
    return status_message_view_;
  }

 protected:
  // InfoBarView:
  int GetContentMinimumWidth() const override;

 private:
  std::unique_ptr<views::View> CreateStatusMessageView(
      content::GlobalRenderFrameHostId shared_tab_id,
      content::GlobalRenderFrameHostId capturer_id,
      const std::u16string& shared_tab_name,
      const std::u16string& capturer_name,
      TabSharingInfoBarDelegate::TabRole role,
      TabSharingInfoBarDelegate::TabShareType capture_type) const;
  std::unique_ptr<views::Label> CreateStatusMessageLabel(
      const TabSharingStatusMessageView::EndpointInfo& shared_tab_info,
      const TabSharingStatusMessageView::EndpointInfo& capturer_info,
      const std::u16string& capturer_name,
      TabSharingInfoBarDelegate::TabRole role,
      TabSharingInfoBarDelegate::TabShareType capture_type) const;
  TabSharingInfoBarDelegate* GetDelegate();

  void StopButtonPressed();
  void ShareThisTabInsteadButtonPressed();
  void QuickNavButtonPressed();
  void OnCapturedSurfaceControlActivityIndicatorPressed();

  // Returns the width of all content other than the label and link.
  // Layout uses this to determine how much space the label and link can take.
  int NonLabelWidth() const;

  // Indicates to the local user which are the capturing and captured origins,
  // and possibly has both as quick-nav links.
  raw_ptr<views::View> status_message_view_;

  raw_ptr<views::MdTextButton> stop_button_ = nullptr;
  raw_ptr<views::MdTextButton> share_this_tab_instead_button_ = nullptr;
  raw_ptr<views::MdTextButton> quick_nav_button_ = nullptr;
  raw_ptr<views::MdTextButton> csc_indicator_button_ = nullptr;
  raw_ptr<views::Link> link_ = nullptr;

  // Facilitates coordinated UMA logging between multiple infobars,
  // ensuring that if the user interacts with one infobars, the other
  // infobars do not mistakenly log "no-interaction".
  // This is owned by TabSharingUIViews.
  const base::WeakPtr<ScreensharingControlsHistogramLogger> uma_logger_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_INFOBAR_H_
