// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/read_later_toolbar_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/webview/webview.h"

namespace {

class ReadLaterSidePanelWebView : public views::WebView,
                                  public BubbleContentsWrapper::Host {
 public:
  ReadLaterSidePanelWebView(Profile* profile, base::RepeatingClosure close_cb)
      : close_cb_(std::move(close_cb)),
        contents_wrapper_(std::make_unique<BubbleContentsWrapperT<ReadLaterUI>>(
            GURL(chrome::kChromeUIReadLaterURL),
            profile,
            IDS_READ_LATER_TITLE,
            /*enable_extension_apis=*/true,
            /*webui_resizes_host=*/false)) {
    contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());
    contents_wrapper_->ReloadWebContents();
    SetWebContents(contents_wrapper_->web_contents());
  }

  // BubbleContentsWrapper::Host:
  void ShowUI() override {}
  void CloseUI() override { close_cb_.Run(); }

 private:
  base::RepeatingClosure close_cb_;
  std::unique_ptr<BubbleContentsWrapperT<ReadLaterUI>> contents_wrapper_;
  base::WeakPtrFactory<ReadLaterSidePanelWebView> weak_factory_{this};
};

}  // namespace

ReadLaterToolbarButton::ReadLaterToolbarButton(Browser* browser)
    : ToolbarButton(base::BindRepeating(&ReadLaterToolbarButton::ButtonPressed,
                                        base::Unretained(this))),
      browser_(browser),
      contents_wrapper_(std::make_unique<BubbleContentsWrapperT<ReadLaterUI>>(
          GURL(chrome::kChromeUIReadLaterURL),
          browser_->profile(),
          IDS_READ_LATER_TITLE,
          true)) {
  contents_wrapper_->ReloadWebContents();

  SetVectorIcon(kReadLaterIcon);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_READ_LATER_TITLE));
}

ReadLaterToolbarButton::~ReadLaterToolbarButton() = default;

void ReadLaterToolbarButton::ButtonPressed() {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_);
  DCHECK(browser_view->side_panel());

  if (!side_panel_webview_) {
    auto webview = std::make_unique<ReadLaterSidePanelWebView>(
        browser_->profile(),
        base::BindRepeating(&ReadLaterToolbarButton::ButtonPressed,
                            base::Unretained(this)));
    side_panel_webview_ =
        browser_view->side_panel()->AddChildView(std::move(webview));
    SetHighlighted(true);
  } else {
    browser_view->side_panel()->RemoveChildViewT(side_panel_webview_);
    side_panel_webview_ = nullptr;
    // TODO(pbos): Observe read_later_side_panel_bubble_ so we don't need to
    // SetHighlighted(false) here.
    SetHighlighted(false);
  }
}
