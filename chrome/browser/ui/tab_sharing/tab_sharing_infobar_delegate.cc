// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace {
using TabRole = ::TabSharingInfoBarDelegate::TabRole;

constexpr int kCapturedSurfaceControlIndicatorButtonIconHeight = 16;
}  // namespace

class TabSharingInfoBarDelegate::TabSharingInfoBarDelegateButton {
 public:
  TabSharingInfoBarDelegateButton() = default;

  // Deleted copy and assignment operator.
  TabSharingInfoBarDelegateButton(const TabSharingInfoBarDelegateButton&) =
      delete;
  TabSharingInfoBarDelegateButton& operator=(
      const TabSharingInfoBarDelegateButton&) = delete;

  virtual ~TabSharingInfoBarDelegateButton() = default;
  virtual void Click(infobars::InfoBar* infobar) = 0;
  virtual std::u16string GetLabel() const = 0;
  virtual ui::ImageModel GetImage() const { return {}; }
  virtual bool IsEnabled() const { return true; }
  virtual std::u16string GetTooltip() const { return u""; }
};

using TabSharingInfoBarDelegateButton =
    ::TabSharingInfoBarDelegate::TabSharingInfoBarDelegateButton;

class TabSharingInfoBarDelegate::StopButton
    : public TabSharingInfoBarDelegateButton {
 public:
  StopButton(TabSharingUI* ui,
             TabSharingInfoBarDelegate::TabShareType capture_type)
      : ui_(ui), capture_type_(capture_type) {}
  ~StopButton() override = default;

  void Click(infobars::InfoBar* infobar) override { ui_->StopSharing(); }

  std::u16string GetLabel() const override {
    switch (capture_type_) {
      case TabSharingInfoBarDelegate::TabShareType::CAST:
        return l10n_util::GetStringUTF16(IDS_TAB_CASTING_INFOBAR_STOP_BUTTON);
      case TabSharingInfoBarDelegate::TabShareType::CAPTURE:
        return l10n_util::GetStringUTF16(IDS_TAB_SHARING_INFOBAR_STOP_BUTTON);
    }
    NOTREACHED();
  }

 private:
  const raw_ptr<TabSharingUI, AcrossTasksDanglingUntriaged> ui_;
  const TabShareType capture_type_;
};

// Represents a button which, when clicked, changes the tab being shared/cast to
// be the current tab (the one associated with this infobar.)
class TabSharingInfoBarDelegate::ShareTabInsteadButton
    : public TabSharingInfoBarDelegateButton {
 public:
  ShareTabInsteadButton(TabSharingUI* ui,
                        TabSharingInfoBarDelegate::ButtonState button_state,
                        TabSharingInfoBarDelegate::TabShareType capture_type)
      : ui_(ui), button_state_(button_state), capture_type_(capture_type) {}

  ~ShareTabInsteadButton() override = default;

  void Click(infobars::InfoBar* infobar) override {
    DCHECK(ui_);  // Not verified in ctor to keep tests simple.
    ui_->StartSharing(infobar);
  }

  std::u16string GetLabel() const override {
    switch (capture_type_) {
      case TabSharingInfoBarDelegate::TabShareType::CAST:
        return l10n_util::GetStringUTF16(IDS_TAB_CASTING_INFOBAR_CAST_BUTTON);
      case TabSharingInfoBarDelegate::TabShareType::CAPTURE:
        return l10n_util::GetStringUTF16(IDS_TAB_SHARING_INFOBAR_SHARE_BUTTON);
    }
    NOTREACHED_IN_MIGRATION();
    return std::u16string();
  }

  bool IsEnabled() const override {
    return button_state_ == TabSharingInfoBarDelegate::ButtonState::ENABLED;
  }

  std::u16string GetTooltip() const override {
    return button_state_ == TabSharingInfoBarDelegate::ButtonState::DISABLED
               ? l10n_util::GetStringUTF16(
                     IDS_POLICY_DLP_SCREEN_SHARE_BLOCKED_TITLE)
               : u"";
  }

 private:
  const raw_ptr<TabSharingUI, AcrossTasksDanglingUntriaged> ui_;
  const TabSharingInfoBarDelegate::ButtonState button_state_;
  const TabSharingInfoBarDelegate::TabShareType capture_type_;
};

// Represents a button which, when clicked, changes the activated tab to be
// the one which was hard-coded into this infobar. The intended use for this
// class is for the captured tab to activate the capturing tab, and vice versa.
class TabSharingInfoBarDelegate::SwitchToTabButton
    : public TabSharingInfoBarDelegateButton {
 public:
  SwitchToTabButton(const TabSharingInfoBarDelegate::FocusTarget& focus_target,
                    bool focus_target_is_capturer)
      : focus_target_(focus_target),
        focus_target_is_capturer_(focus_target_is_capturer) {}
  ~SwitchToTabButton() override = default;

  void Click(infobars::InfoBar* infobar) override {
    content::RenderFrameHost* const rfh =
        content::RenderFrameHost::FromID(focus_target_.id);
    if (!rfh) {
      return;
    }

    page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
        rfh, focus_target_is_capturer_
                 ? blink::mojom::WebFeature::kTabSharingBarSwitchToCapturer
                 : blink::mojom::WebFeature::kTabSharingBarSwitchToCapturee);

    content::WebContents* const web_contents =
        content::WebContents::FromRenderFrameHost(rfh);
    DCHECK(web_contents);

    web_contents->GetDelegate()->ActivateContents(web_contents);
    Browser* const browser = chrome::FindBrowserWithTab(web_contents);
    if (browser && browser->window()) {
      browser->window()->Activate();
    }
  }

  std::u16string GetLabel() const override {
    // TODO(crbug.com/40188004): Hard-code this text into the button.
    content::RenderFrameHost* const rfh =
        content::RenderFrameHost::FromID(focus_target_.id);
    if (!rfh) {
      return GetDefaultLabel();
    }
    return l10n_util::GetStringFUTF16(
        IDS_TAB_SHARING_INFOBAR_SWITCH_TO_BUTTON,
        url_formatter::FormatOriginForSecurityDisplay(
            rfh->GetLastCommittedOrigin(),
            url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
  }

  ui::ImageModel GetImage() const override { return focus_target_.icon; }

 private:
  std::u16string GetDefaultLabel() const {
    return l10n_util::GetStringUTF16(
        focus_target_is_capturer_
            ? IDS_TAB_SHARING_INFOBAR_SWITCH_TO_CAPTURER_BUTTON
            : IDS_TAB_SHARING_INFOBAR_SWITCH_TO_CAPTURED_BUTTON);
  }

  const TabSharingInfoBarDelegate::FocusTarget focus_target_;
  const bool focus_target_is_capturer_;
};

class TabSharingInfoBarDelegate::CscIndicatorButton
    : public TabSharingInfoBarDelegateButton {
 public:
  explicit CscIndicatorButton(content::WebContents* web_contents)
      : web_contents_(web_contents ? web_contents->GetWeakPtr() : nullptr) {}
  ~CscIndicatorButton() override = default;

  void Click(infobars::InfoBar* infobar) override {
    if (!web_contents_) {
      return;
    }
    ShowPageInfoDialog(web_contents_.get(), base::DoNothing());
  }

  std::u16string GetLabel() const override {
    return l10n_util::GetStringUTF16(
        IDS_TAB_SHARING_INFOBAR_CAPTURED_SURFACE_CONTROL_PERMISSION_BUTTON);
  }

  bool IsEnabled() const override { return true; }

  ui::ImageModel GetImage() const override {
    return ui::ImageModel::FromVectorIcon(
        vector_icons::kTouchpadMouseIcon, ui::kColorSysPrimary,
        kCapturedSurfaceControlIndicatorButtonIconHeight);
  }

 private:
  const base::WeakPtr<content::WebContents> web_contents_;
};

namespace {

std::u16string GetMessageTextCastingNoSinkName(
    bool shared_tab,
    const std::u16string& shared_tab_name) {
  if (shared_tab) {
    return l10n_util::GetStringUTF16(
        IDS_TAB_CASTING_INFOBAR_CASTING_CURRENT_TAB_NO_DEVICE_NAME_LABEL);
  }
  return shared_tab_name.empty()
             ? l10n_util::GetStringUTF16(
                   IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_UNTITLED_TAB_NO_DEVICE_NAME_LABEL)
             : l10n_util::GetStringFUTF16(
                   IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_TAB_NO_DEVICE_NAME_LABEL,
                   shared_tab_name);
}

std::u16string GetMessageTextCasting(bool shared_tab,
                                     const std::u16string& shared_tab_name,
                                     const std::u16string& sink_name) {
  if (sink_name.empty()) {
    return GetMessageTextCastingNoSinkName(shared_tab, shared_tab_name);
  }

  if (shared_tab) {
    return l10n_util::GetStringFUTF16(
        IDS_TAB_CASTING_INFOBAR_CASTING_CURRENT_TAB_LABEL, sink_name);
  }
  return shared_tab_name.empty()
             ? l10n_util::GetStringFUTF16(
                   IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_UNTITLED_TAB_LABEL,
                   sink_name)
             : l10n_util::GetStringFUTF16(
                   IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_TAB_LABEL,
                   shared_tab_name, sink_name);
}

std::u16string GetMessageTextCapturing(bool shared_tab,
                                       const std::u16string& shared_tab_name,
                                       const std::u16string& app_name) {
  if (shared_tab) {
    return l10n_util::GetStringFUTF16(
        IDS_TAB_SHARING_INFOBAR_SHARING_CURRENT_TAB_LABEL, app_name);
  }
  return !shared_tab_name.empty()
             ? l10n_util::GetStringFUTF16(
                   IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_TAB_LABEL,
                   shared_tab_name, app_name)
             : l10n_util::GetStringFUTF16(
                   IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_UNTITLED_TAB_LABEL,
                   app_name);
}

bool IsCapturedTab(TabRole role) {
  switch (role) {
    case TabRole::kCapturingTab:
    case TabRole::kOtherTab:
      return false;
    case TabRole::kCapturedTab:
    case TabRole::kSelfCapturingTab:
      return true;
  }
  NOTREACHED();
}

}  // namespace

// static
infobars::InfoBar* TabSharingInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager,
    infobars::InfoBar* old_infobar,
    const std::u16string& shared_tab_name,
    const std::u16string& capturer_name,
    content::WebContents* web_contents,
    TabRole role,
    ButtonState share_this_tab_instead_button_state,
    std::optional<FocusTarget> focus_target,
    bool captured_surface_control_active,
    TabSharingUI* ui,
    TabShareType capture_type,
    bool favicons_used_for_switch_to_tab_button) {
  DCHECK(infobar_manager);
  std::unique_ptr<infobars::InfoBar> new_infobar =
      CreateTabSharingInfoBar(base::WrapUnique(new TabSharingInfoBarDelegate(
          shared_tab_name, capturer_name, web_contents, role,
          share_this_tab_instead_button_state, focus_target,
          captured_surface_control_active, ui, capture_type,
          favicons_used_for_switch_to_tab_button)));
  return old_infobar ? infobar_manager->ReplaceInfoBar(old_infobar,
                                                       std::move(new_infobar))
                     : infobar_manager->AddInfoBar(std::move(new_infobar));
}

TabSharingInfoBarDelegate::TabSharingInfoBarDelegate(
    std::u16string shared_tab_name,
    std::u16string capturer_name,
    content::WebContents* web_contents,
    TabRole role,
    ButtonState share_this_tab_instead_button_state,
    std::optional<FocusTarget> focus_target,
    bool captured_surface_control_active,
    TabSharingUI* ui,
    TabShareType capture_type,
    bool favicons_used_for_switch_to_tab_button)
    : shared_tab_name_(std::move(shared_tab_name)),
      role_(role),
      capturer_name_(std::move(capturer_name)),
      ui_(ui),
      favicons_used_for_switch_to_tab_button_(
          favicons_used_for_switch_to_tab_button),
      capture_type_(capture_type) {
  stop_button_ = std::make_unique<StopButton>(ui_, capture_type_);

  if (share_this_tab_instead_button_state != ButtonState::NOT_SHOWN) {
    share_this_tab_instead_button_ = std::make_unique<ShareTabInsteadButton>(
        ui_, share_this_tab_instead_button_state, capture_type_);
  }

  if (focus_target.has_value()) {
    quick_nav_button_ = std::make_unique<SwitchToTabButton>(
        *focus_target, IsCapturedTab(role_));
  }

  // Note that kSelfCapturingTab is intentionally disregarded,
  // because write-access CapturedSurfaceControl APIs are disallowed
  // in that case anyway.
  //
  // TODO(crbug.com/324468211): Hide the button if Captured Surface Control
  // is set to BLOCKED or ASK through the user's interaction with PageInfo.
  if (role_ == TabRole::kCapturingTab && captured_surface_control_active &&
      base::FeatureList::IsEnabled(
          features::kCapturedSurfaceControlStickyPermissions)) {
    csc_indicator_button_ = std::make_unique<CscIndicatorButton>(web_contents);
  }
}

TabSharingInfoBarDelegate::~TabSharingInfoBarDelegate() = default;

bool TabSharingInfoBarDelegate::EqualsDelegate(
    InfoBarDelegate* delegate) const {
  return false;
}

bool TabSharingInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

infobars::InfoBarDelegate::InfoBarIdentifier
TabSharingInfoBarDelegate::GetIdentifier() const {
  return TAB_SHARING_INFOBAR_DELEGATE;
}

std::u16string TabSharingInfoBarDelegate::GetMessageText() const {
  switch (capture_type_) {
    case TabShareType::CAST:
      return GetMessageTextCasting(IsCapturedTab(role_), shared_tab_name_,
                                   capturer_name_);
    case TabShareType::CAPTURE:
      return GetMessageTextCapturing(IsCapturedTab(role_), shared_tab_name_,
                                     capturer_name_);
  }
  NOTREACHED();
}

std::u16string TabSharingInfoBarDelegate::GetButtonLabel(
    TabSharingInfoBarButton button) const {
  return GetButton(button).GetLabel();
}

ui::ImageModel TabSharingInfoBarDelegate::GetButtonImage(
    TabSharingInfoBarButton button) const {
  return GetButton(button).GetImage();
}

bool TabSharingInfoBarDelegate::IsButtonEnabled(
    TabSharingInfoBarButton button) const {
  return GetButton(button).IsEnabled();
}

std::u16string TabSharingInfoBarDelegate::GetButtonTooltip(
    TabSharingInfoBarButton button) const {
  return GetButton(button).GetTooltip();
}

int TabSharingInfoBarDelegate::GetButtons() const {
  return (stop_button_ ? kStop : 0) |
         (share_this_tab_instead_button_ ? kShareThisTabInstead : 0) |
         (quick_nav_button_ ? kQuickNav : 0) |
         (csc_indicator_button_ ? kCapturedSurfaceControlIndicator : 0);
}

void TabSharingInfoBarDelegate::Stop() {
  GetButton(kStop).Click(infobar());
}

void TabSharingInfoBarDelegate::ShareThisTabInstead() {
  GetButton(kShareThisTabInstead).Click(infobar());
}

void TabSharingInfoBarDelegate::QuickNav() {
  GetButton(kQuickNav).Click(infobar());
}

void TabSharingInfoBarDelegate::
    OnCapturedSurfaceControlActivityIndicatorPressed() {
  GetButton(kCapturedSurfaceControlIndicator).Click(infobar());
}

bool TabSharingInfoBarDelegate::IsCloseable() const {
  return false;
}

const gfx::VectorIcon& TabSharingInfoBarDelegate::GetVectorIcon() const {
  return vector_icons::kScreenShareIcon;
}

const TabSharingInfoBarDelegateButton& TabSharingInfoBarDelegate::GetButton(
    TabSharingInfoBarButton button) const {
  switch (button) {
    case TabSharingInfoBarButton::kNone:
      break;
    case TabSharingInfoBarButton::kStop:
      return *stop_button_;
    case TabSharingInfoBarButton::kShareThisTabInstead:
      return *share_this_tab_instead_button_;
    case TabSharingInfoBarButton::kQuickNav:
      return *quick_nav_button_;
    case TabSharingInfoBarButton::kCapturedSurfaceControlIndicator:
      return *csc_indicator_button_;
  }
  NOTREACHED();
}

TabSharingInfoBarDelegateButton& TabSharingInfoBarDelegate::GetButton(
    TabSharingInfoBarButton button) {
  return const_cast<TabSharingInfoBarDelegateButton&>(
      const_cast<const TabSharingInfoBarDelegate*>(this)->GetButton(button));
}
