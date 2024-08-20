// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/storage/storage_pressure_bubble_view.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/common/content_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/layout/box_layout.h"

namespace {

const char kAllSitesContentSettingsUrl[] =
    "chrome://settings/content/all?sort=data-stored";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class StoragePressureBubbleHistogramValue {
  kShown = 0,
  kIgnored = 1,
  kOpenedAllSites = 2,
  kMaxValue = kOpenedAllSites,
};

void RecordBubbleHistogramValue(StoragePressureBubbleHistogramValue value) {
  base::UmaHistogramEnumeration("Storage.StoragePressure.Bubble", value);
}

}  // namespace

namespace chrome {

// static
void ShowStoragePressureBubble(const url::Origin& origin) {
  StoragePressureBubbleView::ShowBubble(origin);
}

}  // namespace chrome

void StoragePressureBubbleView::ShowBubble(const url::Origin& origin) {
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (!browser)
    return;

  StoragePressureBubbleView* bubble = new StoragePressureBubbleView(
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider()
          ->GetAppMenuButton(),
      browser, origin);
  views::BubbleDialogDelegateView::CreateBubble(bubble)->Show();

  RecordBubbleHistogramValue(StoragePressureBubbleHistogramValue::kShown);
}

StoragePressureBubbleView::StoragePressureBubbleView(views::View* anchor_view,
                                                     Browser* browser,
                                                     const url::Origin& origin)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      browser_(browser),
      origin_(origin),
      ignored_(true) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetTitle(IDS_SETTINGS_STORAGE_PRESSURE_BUBBLE_VIEW_TITLE);
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_SETTINGS_STORAGE_PRESSURE_BUBBLE_VIEW_BUTTON_LABEL));
  SetAcceptCallback(base::BindOnce(&StoragePressureBubbleView::OnDialogAccepted,
                                   base::Unretained(this)));
  set_close_on_deactivate(false);
}

StoragePressureBubbleView::~StoragePressureBubbleView() {
  if (ignored_) {
    RecordBubbleHistogramValue(StoragePressureBubbleHistogramValue::kIgnored);
  }
}

void StoragePressureBubbleView::OnDialogAccepted() {
  ignored_ = false;
  RecordBubbleHistogramValue(
      StoragePressureBubbleHistogramValue::kOpenedAllSites);
  // TODO(ellyjones): What is this doing here? The widget's about to close
  // anyway?
  GetWidget()->Close();
  const GURL all_sites_gurl(kAllSitesContentSettingsUrl);
  NavigateParams params(browser_, all_sites_gurl,
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
