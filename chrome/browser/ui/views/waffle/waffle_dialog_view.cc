// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/waffle/waffle_dialog_view.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/waffle/waffle_tab_helper.h"
#include "chrome/browser/ui/webui/waffle/waffle_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace {
// Temporary until the mocks are ready.
constexpr int kDialogWidth = 800;
constexpr int kDialogHeight = 600;
}  // namespace

void ShowWaffleDialog(Browser& browser) {
  auto delegate = std::make_unique<views::DialogDelegate>();
  delegate->SetButtons(ui::DIALOG_BUTTON_NONE);
  delegate->SetModalType(ui::MODAL_TYPE_WINDOW);
  delegate->SetShowCloseButton(true);
  delegate->SetOwnedByWidget(true);

  auto waffleDialogView = std::make_unique<WaffleDialogView>(&browser);
  waffleDialogView->Initialize();
  delegate->SetContentsView(std::move(waffleDialogView));

  constrained_window::CreateBrowserModalDialogViews(
      std::move(delegate), browser.window()->GetNativeWindow());
}

WaffleDialogView::WaffleDialogView(Browser* browser) : browser_(browser) {
  CHECK(base::FeatureList::IsEnabled(kWaffle));
  // Create the web view in the native dialog.
  web_view_ =
      AddChildView(std::make_unique<views::WebView>(browser->profile()));
}

void WaffleDialogView::Initialize() {
  web_view_->LoadInitialURL(GURL(chrome::kChromeUIWaffleURL));

  const int max_width = browser_->window()
                            ->GetWebContentsModalDialogHost()
                            ->GetMaximumDialogSize()
                            .width();
  const int width =
      views::LayoutProvider::Get()->GetSnappedDialogWidth(kDialogWidth);
  web_view_->SetPreferredSize(
      gfx::Size(std::min(width, max_width), kDialogHeight));

  auto* web_ui = web_view_->GetWebContents()
                     ->GetWebUI()
                     ->GetController()
                     ->GetAs<WaffleUI>();
  CHECK(web_ui);
  // Unretained is fine because this outlives the inner web UI.
  web_ui->Initialize(base::BindOnce(&WaffleDialogView::ShowNativeView,
                                    base::Unretained(this)));

  SetUseDefaultFillLayout(true);
}

void WaffleDialogView::ShowNativeView() {
  GetWidget()->Show();
  web_view_->RequestFocus();
}

BEGIN_METADATA(WaffleDialogView, views::View)
END_METADATA
