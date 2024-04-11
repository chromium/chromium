// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_INFOBAR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"

namespace views {
class Label;
class MdTextButton;
}  // namespace views

class TabSharingInfoBarDelegate;

// The infobar displayed when sharing a tab. It shows:
// - a message informing the user about which site is shared with which site
// - an optional button for quick navigation to the capturing/captured tab
// - an optional button for sharing the currently displayed tab instead
// - a button to stop the capture
class TabSharingInfoBar : public InfoBarView {
 public:
  explicit TabSharingInfoBar(
      std::unique_ptr<TabSharingInfoBarDelegate> delegate);

  TabSharingInfoBar(const TabSharingInfoBar&) = delete;
  TabSharingInfoBar& operator=(const TabSharingInfoBar&) = delete;

  ~TabSharingInfoBar() override;

  // InfoBarView:
  void Layout(PassKey) override;

  views::MdTextButton* stop_button_for_testing() { return stop_button_; }

 protected:
  // InfoBarView:
  int GetContentMinimumWidth() const override;

 private:
  TabSharingInfoBarDelegate* GetDelegate();

  void StopButtonPressed();
  void ShareThisTabInsteadButtonPressed();
  void QuickNavButtonPressed();
  void OnCapturedSurfaceControlActivityIndicatorPressed();

  // Returns the width of all content other than the label and link.
  // Layout uses this to determine how much space the label and link can take.
  int NonLabelWidth() const;

  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::MdTextButton> stop_button_ = nullptr;
  raw_ptr<views::MdTextButton> share_this_tab_instead_button_ = nullptr;
  raw_ptr<views::MdTextButton> quick_nav_button_ = nullptr;
  raw_ptr<views::MdTextButton> csc_indicator_button_ = nullptr;
  raw_ptr<views::Link> link_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_INFOBAR_H_
