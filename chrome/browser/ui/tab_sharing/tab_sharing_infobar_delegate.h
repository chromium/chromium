// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_INFOBAR_DELEGATE_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/global_routing_id.h"

namespace content {
class WebContents;
}

namespace infobars {
class InfoBarManager;
class InfoBar;
}  // namespace infobars

class ScreensharingControlsHistogramLogger;
class TabSharingUI;

// Creates an infobar for sharing a tab using desktopCapture() API; one delegate
// per tab.
//
// 1. Layout for currently shared tab:
// "Sharing this tab to |capturer_name_| [Stop]"
//
// 2. Layout for capturing/captured tab:
// "Sharing |shared_tab_name_| to |capturer_name_| [Stop] [Switch-Label]"
// Where [Switch-Label] is "Switch to tab <hostname>", with the hostname in
// the captured tab being the capturer's, and vice versa.
//
// 3a. Layout for all other tabs:
// "Sharing |shared_tab_name_| to |capturer_name_| [Stop] [Share this tab
// instead]"
// 3b. Or if |shared_tab_name_| is empty:
// "Sharing a tab to |capturer_name_| [Stop] [Share this tab instead]"
class TabSharingInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  enum TabSharingInfoBarButton {
    kNone = 0,
    kStop = 1 << 0,
    kShareThisTabInstead = 1 << 1,
    kQuickNav = 1 << 2,
    kCapturedSurfaceControlIndicator = 1 << 3,
  };

  enum class ButtonState {
    ENABLED,
    DISABLED,
    NOT_SHOWN,
  };

  // The user-facing mechanism that initiated the tab capture influences the UX
  // elements and language that should be presented to the user.
  // CAPTURE: getDisplayMedia, usually goes with "share".
  // CAST: Chromecasting, usually goes with "cast".
  enum class TabShareType {
    CAPTURE,
    CAST,
  };

  enum class TabRole {
    kCapturingTab,
    kCapturedTab,
    kSelfCapturingTab,  // kCapturingTab && kCapturedTab
    kOtherTab,
  };

  static bool IsCapturedTab(TabRole role);
  static bool IsCapturingTab(TabRole role);

  class TabSharingInfoBarDelegateButton;
  class StopButton;
  class ShareTabInsteadButton;
  class SwitchToTabButton;
  class CscIndicatorButton;

  // Creates a tab sharing infobar, which has 1-2 buttons.
  //
  // The primary button is for stopping the capture. It is always present.
  //
  // The infobar replaces any non-null `old_infobar`, or will be added if that
  // is null.
  //
  // |role| denotes whether the infobar is associated with the capturing
  // and/or captured tab.
  //
  // If |focus_target| has a value, the secondary button switches focus.
  // The image on the secondary button is derived from |focus_target|.
  // Else, if |can_share_instead|, the secondary button changes the
  // capture target to be the tab associated with |this| object.
  // Otherwise, there is no secondary button.
  static infobars::InfoBar* Create(
      infobars::InfoBarManager* infobar_manager,
      infobars::InfoBar* old_infobar,
      content::GlobalRenderFrameHostId shared_tab_id,
      content::GlobalRenderFrameHostId capturer_id,
      const std::u16string& shared_tab_name,
      const std::u16string& capturer_name,
      content::WebContents* web_contents,
      TabRole role,
      ButtonState share_this_tab_instead_button_state,
      content::GlobalRenderFrameHostId focus_target,
      bool captured_surface_control_active,
      TabSharingUI* ui,
      TabShareType capture_type);

  ~TabSharingInfoBarDelegate() override;

  // TODO(crbug.com/40188004): Inline these methods into TabSharingInfoBar where
  // feasible or add comments to document their function better.
  std::u16string GetButtonLabel(TabSharingInfoBarButton button) const;
  ui::ImageModel GetButtonImage(TabSharingInfoBarButton button) const;
  bool IsButtonEnabled(TabSharingInfoBarButton button) const;
  std::u16string GetButtonTooltip(TabSharingInfoBarButton button) const;
  int GetButtons() const;

  void Stop();
  void ShareThisTabInstead();
  void QuickNav();
  void OnCapturedSurfaceControlActivityIndicatorPressed();

  // InfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  bool EqualsDelegate(InfoBarDelegate* delegate) const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool IsCloseable() const override;
  infobars::InfoBarDelegate::InfobarPriority GetPriority() const override;

 private:
  TabSharingInfoBarDelegate(content::WebContents* web_contents,
                            TabRole role,
                            ButtonState share_this_tab_instead_button_state,
                            content::GlobalRenderFrameHostId focus_target,
                            bool captured_surface_control_active,
                            TabSharingUI* ui,
                            TabShareType capture_type);

  const TabSharingInfoBarDelegateButton& GetButton(
      TabSharingInfoBarButton button) const;
  TabSharingInfoBarDelegateButton& GetButton(TabSharingInfoBarButton button);

  // Creates and removes delegate's infobar; outlives delegate.
  const raw_ptr<TabSharingUI, AcrossTasksDanglingUntriaged> ui_;

  // Indicates whether this instance is used for casting or capturing.
  const TabShareType capture_type_;

  std::unique_ptr<StopButton> stop_button_;
  std::unique_ptr<ShareTabInsteadButton> share_this_tab_instead_button_;
  std::unique_ptr<SwitchToTabButton> quick_nav_button_;
  std::unique_ptr<CscIndicatorButton> csc_indicator_button_;
};

std::unique_ptr<infobars::InfoBar> CreateTabSharingInfoBar(
    std::unique_ptr<TabSharingInfoBarDelegate> delegate,
    content::GlobalRenderFrameHostId shared_tab_id,
    content::GlobalRenderFrameHostId capturer_id,
    const std::u16string& shared_tab_name,
    const std::u16string& capturer_name,
    TabSharingInfoBarDelegate::TabRole role,
    TabSharingInfoBarDelegate::TabShareType capture_type,
    base::WeakPtr<ScreensharingControlsHistogramLogger> uma_logger);

#endif  // CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_INFOBAR_DELEGATE_H_
