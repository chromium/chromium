// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_qrcode_infobar.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/signin/signin_qrcode_infobar_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

SigninQRCodeInfoBar::SigninQRCodeInfoBar(
    Profile* profile,
    std::unique_ptr<SigninQRCodeInfoBarDelegate> delegate)
    : InfoBarView(std::move(delegate)) {
  auto web_view = std::make_unique<views::WebView>(profile);
  web_view->GetWebContents()->GetController().LoadURL(
      GURL(chrome::kChromeUISigninQRCodeBarURL), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());

  // Make WebView flexible inside content_container_
  web_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  web_view_ = AddContentChildView(std::move(web_view));

  SetInteriorMargin(gfx::Insets());

  // Set target height to 100 to override default infobar height
  SetTargetHeight(100);
}

SigninQRCodeInfoBar::~SigninQRCodeInfoBar() = default;

void SigninQRCodeInfoBar::PlatformSpecificShow(bool animate) {
  InfoBarView::PlatformSpecificShow(animate);
  if (parent()) {
    views::View* shadow = parent()->children().back().get();
    if (shadow) {
      shadow->SetVisible(false);
    }
  }
}

BEGIN_METADATA(SigninQRCodeInfoBar)
END_METADATA
