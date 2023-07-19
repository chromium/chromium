// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
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
#include "content/public/browser/web_contents_delegate.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "ui/base/l10n/l10n_util.h"

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

namespace {

// Represents a button which, when clicked, changes the tab being shared/cast to
// be the current tab (the one associated with this infobar.)
class ShareTabInsteadButton
    : public TabSharingInfoBarDelegate::TabSharingInfoBarDelegateButton {
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
    NOTREACHED();
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
class SwitchToTabButton
    : public TabSharingInfoBarDelegate::TabSharingInfoBarDelegateButton {
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
    Browser* const browser = chrome::FindBrowserWithWebContents(web_contents);
    if (browser && browser->window()) {
      browser->window()->Activate();
    }
  }

  std::u16string GetLabel() const override {
    // TODO(crbug.com/1224363): Hard-code this text into the button.
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

}  // namespace

// static
infobars::InfoBar* TabSharingInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager,
    const std::u16string& shared_tab_name,
    const std::u16string& capturer_name,
    bool shared_tab,
    ButtonState share_this_tab_instead_button_state,
    absl::optional<FocusTarget> focus_target,
    TabSharingUI* ui,
    TabShareType capture_type,
    bool favicons_used_for_switch_to_tab_button) {
  DCHECK(infobar_manager);
  return infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(base::WrapUnique(new TabSharingInfoBarDelegate(
          shared_tab_name, capturer_name, shared_tab,
          share_this_tab_instead_button_state, focus_target, ui, capture_type,
          favicons_used_for_switch_to_tab_button))));
}

TabSharingInfoBarDelegate::TabSharingInfoBarDelegate(
    std::u16string shared_tab_name,
    std::u16string capturer_name,
    bool shared_tab,
    ButtonState share_this_tab_instead_button_state,
    absl::optional<FocusTarget> focus_target,
    TabSharingUI* ui,
    TabShareType capture_type,
    bool favicons_used_for_switch_to_tab_button)
    : shared_tab_name_(std::move(shared_tab_name)),
      shared_tab_(shared_tab),
      capturer_name_(std::move(capturer_name)),
      ui_(ui),
      favicons_used_for_switch_to_tab_button_(
          favicons_used_for_switch_to_tab_button),
      capture_type_(capture_type) {
  auto share_this_tab_instead_button =
      share_this_tab_instead_button_state != ButtonState::NOT_SHOWN
          ? std::make_unique<ShareTabInsteadButton>(
                ui_, share_this_tab_instead_button_state, capture_type_)
          : nullptr;

  auto quick_nav =
      focus_target.has_value()
          ? std::make_unique<SwitchToTabButton>(*focus_target, shared_tab)
          : nullptr;

  if (share_this_tab_instead_button && quick_nav) {
    // [Stop] [Share-this-tab-instead] [View tab: ...]
    secondary_button_ = std::move(share_this_tab_instead_button);
    tertiary_button_ = std::move(quick_nav);
  } else if (share_this_tab_instead_button) {
    // [Stop] [Share-this-tab-instead]
    secondary_button_ = std::move(share_this_tab_instead_button);
  } else if (quick_nav) {
    // [Stop] [View tab: ...]
    secondary_button_ = std::move(quick_nav);
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
      return GetMessageTextCasting(shared_tab_, shared_tab_name_,
                                   capturer_name_);
    case TabShareType::CAPTURE:
      return GetMessageTextCapturing(shared_tab_, shared_tab_name_,
                                     capturer_name_);
  }
  NOTREACHED();
  return std::u16string();
}

std::u16string TabSharingInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  if (button == BUTTON_OK) {
    switch (capture_type_) {
      case TabSharingInfoBarDelegate::TabShareType::CAST:
        return l10n_util::GetStringUTF16(IDS_TAB_CASTING_INFOBAR_STOP_BUTTON);
      case TabSharingInfoBarDelegate::TabShareType::CAPTURE:
        return l10n_util::GetStringUTF16(IDS_TAB_SHARING_INFOBAR_STOP_BUTTON);
    }
    NOTREACHED();
    return std::u16string();
  } else if (button == BUTTON_CANCEL) {
    DCHECK(secondary_button_);
    return secondary_button_->GetLabel();
  } else if (button == BUTTON_EXTRA) {
    DCHECK(tertiary_button_);
    return tertiary_button_->GetLabel();
  }
  return ConfirmInfoBarDelegate::GetButtonLabel(button);
}

ui::ImageModel TabSharingInfoBarDelegate::GetButtonImage(
    InfoBarButton button) const {
  if (favicons_used_for_switch_to_tab_button_) {
    if (button == BUTTON_CANCEL) {
      DCHECK(secondary_button_);
      return secondary_button_->GetImage();
    } else if (button == BUTTON_EXTRA) {
      DCHECK(tertiary_button_);
      return tertiary_button_->GetImage();
    }
  }
  return ConfirmInfoBarDelegate::GetButtonImage(button);
}

bool TabSharingInfoBarDelegate::GetButtonEnabled(InfoBarButton button) const {
  if (button == BUTTON_CANCEL) {
    DCHECK(secondary_button_);
    return secondary_button_->IsEnabled();
  } else if (button == BUTTON_EXTRA) {
    DCHECK(tertiary_button_);
    return tertiary_button_->IsEnabled();
  }
  return ConfirmInfoBarDelegate::GetButtonEnabled(button);
}

std::u16string TabSharingInfoBarDelegate::GetButtonTooltip(
    InfoBarButton button) const {
  if (button == BUTTON_CANCEL) {
    DCHECK(secondary_button_);
    return secondary_button_->GetTooltip();
  } else if (button == BUTTON_EXTRA) {
    DCHECK(tertiary_button_);
    return tertiary_button_->GetTooltip();
  }
  return ConfirmInfoBarDelegate::GetButtonTooltip(button);
}

int TabSharingInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | (secondary_button_ ? BUTTON_CANCEL : 0) |
         (tertiary_button_ ? BUTTON_EXTRA : 0);
}

bool TabSharingInfoBarDelegate::Accept() {
  ui_->StopSharing();
  return false;
}

bool TabSharingInfoBarDelegate::Cancel() {
  DCHECK(secondary_button_);
  secondary_button_->Click(infobar());
  return false;
}

bool TabSharingInfoBarDelegate::ExtraButtonPressed() {
  DCHECK(tertiary_button_);
  tertiary_button_->Click(infobar());
  return false;
}

bool TabSharingInfoBarDelegate::IsCloseable() const {
  return false;
}

const gfx::VectorIcon& TabSharingInfoBarDelegate::GetVectorIcon() const {
  return vector_icons::kScreenShareIcon;
}
