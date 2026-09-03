// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/storage/storage_pressure_bubble_view.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/storage_pressure_bubble.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/common/content_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/layout/box_layout.h"

namespace {

const char kAllSitesContentSettingsUrl[] =
    "chrome://settings/content/all?sort=data-stored";

}  // namespace

// static
void ShowStoragePressureBubble(const url::Origin& origin) {
  StoragePressureBubbleView::ShowBubble(origin);
}

void StoragePressureBubbleView::ShowBubble(const url::Origin& origin) {
  BrowserWindowInterface* const bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  if (!bwi) {
    return;
  }

  auto* browser_view = BrowserView::GetBrowserViewForBrowser(bwi);
  auto* control = browser_view->toolbar_button_provider()->GetAppMenuControl();
  views::BubbleAnchor anchor =
      control ? control->GetAnchor() : views::BubbleAnchor();
  StoragePressureBubbleView* bubble =
      new StoragePressureBubbleView(anchor, bwi, origin);
  views::BubbleDialogDelegateView::CreateBubble(bubble)->Show();
}

StoragePressureBubbleView::StoragePressureBubbleView(
    views::BubbleAnchor anchor,
    BrowserWindowInterface* bwi,
    const url::Origin& origin)
    : BubbleDialogDelegateView(anchor, views::BubbleBorder::TOP_RIGHT),
      bwi_(bwi),
      origin_(origin) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetTitle(IDS_SETTINGS_STORAGE_PRESSURE_BUBBLE_VIEW_TITLE);
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_SETTINGS_STORAGE_PRESSURE_BUBBLE_VIEW_BUTTON_LABEL));
  SetAcceptCallback(base::BindOnce(&StoragePressureBubbleView::OnDialogAccepted,
                                   base::Unretained(this)));
  set_close_on_deactivate(false);
}

StoragePressureBubbleView::~StoragePressureBubbleView() = default;

void StoragePressureBubbleView::OnDialogAccepted() {
  const GURL all_sites_gurl(kAllSitesContentSettingsUrl);
  NavigateParams params(bwi_, all_sites_gurl,
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void StoragePressureBubbleView::Init() {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  // Description text label.
  auto origin_string = url_formatter::FormatUrl(
      origin_.GetURL(),
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitTrivialSubdomains |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrailingSlashOnBareHostname,
      base::UnescapeRule::NONE, nullptr, nullptr, nullptr);
  auto text_label = std::make_unique<views::Label>(l10n_util::GetStringFUTF16(
      IDS_SETTINGS_STORAGE_PRESSURE_BUBBLE_VIEW_MESSAGE, origin_string));
  text_label->SetMultiLine(true);
  text_label->SetLineHeight(20);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label->SizeToFit(
      provider->GetDistanceMetric(views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
      margins().width());
  AddChildView(std::move(text_label));
}

bool StoragePressureBubbleView::ShouldShowCloseButton() const {
  return true;
}

BEGIN_METADATA(StoragePressureBubbleView)
END_METADATA
