// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/network_profile_bubble.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/fill_layout.h"

namespace {

class NetworkProfileBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(NetworkProfileBubbleView, views::BubbleDialogDelegateView)

 public:
  NetworkProfileBubbleView(views::View* anchor,
                           content::PageNavigator* navigator,
                           Profile* profile);
  NetworkProfileBubbleView(const NetworkProfileBubbleView&) = delete;
  NetworkProfileBubbleView& operator=(const NetworkProfileBubbleView&) = delete;

 private:
  ~NetworkProfileBubbleView() override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  bool Accept() override;

  void LinkClicked(const ui::Event&);

  // Used for loading pages.
  raw_ptr<content::PageNavigator> navigator_;
  raw_ptr<Profile> profile_;
};

////////////////////////////////////////////////////////////////////////////////
// NetworkProfileBubbleView, public:

NetworkProfileBubbleView::NetworkProfileBubbleView(
    views::View* anchor,
    content::PageNavigator* navigator,
    Profile* profile)
    : BubbleDialogDelegateView(anchor, views::BubbleBorder::TOP_RIGHT),
      navigator_(navigator),
      profile_(profile) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  auto* learn_more = SetExtraView(
      std::make_unique<views::Link>(l10n_util::GetStringUTF16(IDS_LEARN_MORE)));
  learn_more->SetCallback(base::BindRepeating(
      &NetworkProfileBubbleView::LinkClicked, base::Unretained(this)));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkProfileBubbleView, private:

NetworkProfileBubbleView::~NetworkProfileBubbleView() {}

void NetworkProfileBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  views::Label* label = new views::Label(
      l10n_util::GetStringFUTF16(IDS_PROFILE_ON_NETWORK_WARNING,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  label->SetMultiLine(true);
  constexpr int kNotificationBubbleWidth = 250;
  label->SizeToFit(kNotificationBubbleWidth);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label);
}

bool NetworkProfileBubbleView::Accept() {
  NetworkProfileBubble::RecordUmaEvent(
      NetworkProfileBubble::METRIC_ACKNOWLEDGED);
  return true;
}

void NetworkProfileBubbleView::LinkClicked(const ui::Event& event) {
  NetworkProfileBubble::RecordUmaEvent(
      NetworkProfileBubble::METRIC_LEARN_MORE_CLICKED);
  WindowOpenDisposition disposition = ui::DispositionFromEventFlags(
      event.flags(), WindowOpenDisposition::NEW_FOREGROUND_TAB);
  content::OpenURLParams params(
      GURL("https://sites.google.com/a/chromium.org/dev/administrators/"
           "common-problems-and-solutions#network_profile"),
      content::Referrer(), disposition, ui::PAGE_TRANSITION_LINK, false);
  navigator_->OpenURL(params, /*navigation_handle_callback=*/{});

  // If the user interacted with the bubble we don't reduce the number of
  // warnings left.
  PrefService* prefs = profile_->GetPrefs();
  int left_warnings = prefs->GetInteger(prefs::kNetworkProfileWarningsLeft);
  prefs->SetInteger(prefs::kNetworkProfileWarningsLeft, ++left_warnings);
  GetWidget()->Close();
}

BEGIN_METADATA(NetworkProfileBubbleView)
END_METADATA

}  // namespace

// static
void NetworkProfileBubble::ShowNotification(Browser* browser) {
  views::View* anchor = NULL;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (browser_view && browser_view->toolbar()) {
    anchor = browser_view->toolbar_button_provider()->GetAppMenuButton();
  }
  NetworkProfileBubbleView* bubble =
      new NetworkProfileBubbleView(anchor, browser, browser->profile());
  views::BubbleDialogDelegateView::CreateBubble(bubble)->Show();

  NetworkProfileBubble::SetNotificationShown(true);

  // Mark the time of the last bubble and reduce the number of warnings left
  // before the next silence period starts.
  PrefService* prefs = browser->profile()->GetPrefs();
  prefs->SetInt64(prefs::kNetworkProfileLastWarningTime,
                  base::Time::Now().ToTimeT());
  int left_warnings = prefs->GetInteger(prefs::kNetworkProfileWarningsLeft);
  if (left_warnings > 0) {
    prefs->SetInteger(prefs::kNetworkProfileWarningsLeft, --left_warnings);
  }
}
