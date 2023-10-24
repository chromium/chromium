// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search_engine_choice/search_engine_choice_dialog_view.h"
#include <algorithm>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/ui/search_engine_choice/search_engine_choice_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/search_engines/search_engine_choice_utils.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace {
// The minimum height and maximum dialog dimensions.
// We don't have a minimum width because operating systems have a minimum width
// for Chrome.
constexpr int kMinHeight = 376;
constexpr int kPreferredMaxDialogWidth = 1077;
constexpr int kPreferredMaxDialogHeight = 768;

}  // namespace
void ShowSearchEngineChoiceDialog(
    Browser& browser,
    absl::optional<gfx::Size> boundary_dimensions_for_test,
    absl::optional<double> zoom_factor_for_test) {
  if (boundary_dimensions_for_test.has_value() ||
      zoom_factor_for_test.has_value()) {
    CHECK_IS_TEST();
  }

  auto delegate = std::make_unique<views::DialogDelegate>();
  delegate->SetButtons(ui::DIALOG_BUTTON_NONE);
  delegate->SetModalType(ui::MODAL_TYPE_WINDOW);
  delegate->SetShowCloseButton(false);
  delegate->SetOwnedByWidget(true);

  auto dialogView = std::make_unique<SearchEngineChoiceDialogView>(
      &browser, boundary_dimensions_for_test, zoom_factor_for_test);
  dialogView->Initialize();
  delegate->SetContentsView(std::move(dialogView));

  constrained_window::CreateBrowserModalDialogViews(
      std::move(delegate), browser.window()->GetNativeWindow());
}

SearchEngineChoiceDialogView::SearchEngineChoiceDialogView(
    Browser* browser,
    absl::optional<gfx::Size> boundary_dimensions_for_test,
    absl::optional<double> zoom_factor_for_test)
    : browser_(browser),
      boundary_dimensions_for_test_(boundary_dimensions_for_test),
      zoom_factor_for_test_(zoom_factor_for_test) {
  CHECK(browser_);
  CHECK(search_engines::IsChoiceScreenFlagEnabled(
      search_engines::ChoicePromo::kDialog));
  if (boundary_dimensions_for_test.has_value() ||
      zoom_factor_for_test_.has_value()) {
    CHECK_IS_TEST();
  }

  // Create the web view in the native dialog.
  web_view_ =
      AddChildView(std::make_unique<views::WebView>(browser->profile()));
}

SearchEngineChoiceDialogView::~SearchEngineChoiceDialogView() = default;

void SearchEngineChoiceDialogView::Initialize() {
  auto* search_engine_choice_service =
      SearchEngineChoiceServiceFactory::GetForProfile(browser_->profile());
  search_engine_choice_service->NotifyDialogOpened(
      browser_, /*close_dialog_callback=*/base::BindOnce(
          &SearchEngineChoiceDialogView::CloseView,
          weak_ptr_factory_.GetWeakPtr()));

  web_view_->LoadInitialURL(GURL(chrome::kChromeUISearchEngineChoiceURL));

  double zoom_factor = zoom_factor_for_test_.value_or(1.);
  content::WebContents* web_contents = web_view_->GetWebContents();
  content::RenderFrameHost* render_frame_host =
      web_contents->GetPrimaryMainFrame();
  content::HostZoomMap* zoom_map =
      content::HostZoomMap::GetForWebContents(web_contents);
  zoom_map->SetTemporaryZoomLevel(
      render_frame_host->GetGlobalId(),
      blink::PageZoomFactorToZoomLevel(zoom_factor));

  int preferred_dialog_width = kPreferredMaxDialogWidth;
  int preferred_dialog_height = kPreferredMaxDialogHeight;

  // Use boundary dimensions if initialized to set the preferred dialog width
  // and height.
  if (boundary_dimensions_for_test_.has_value()) {
    preferred_dialog_width = boundary_dimensions_for_test_->width();
    preferred_dialog_height = boundary_dimensions_for_test_->height();
  }

  int max_width = browser_->window()
                      ->GetWebContentsModalDialogHost()
                      ->GetMaximumDialogSize()
                      .width();
  int max_height = browser_->window()
                       ->GetWebContentsModalDialogHost()
                       ->GetMaximumDialogSize()
                       .height();

  const int width = views::LayoutProvider::Get()->GetSnappedDialogWidth(
      preferred_dialog_width);

  const int height =
      std::clamp(preferred_dialog_height, std::min(kMinHeight, max_height),
                 std::max(kMinHeight, max_height));

  web_view_->SetPreferredSize(gfx::Size(std::min(width, max_width), height));

  auto* web_ui = web_view_->GetWebContents()
                     ->GetWebUI()
                     ->GetController()
                     ->GetAs<SearchEngineChoiceUI>();
  CHECK(web_ui);
  // Unretained is fine because this outlives the inner web UI.
  web_ui->Initialize(base::BindOnce(
      &SearchEngineChoiceDialogView::ShowNativeView, base::Unretained(this)));

  SetUseDefaultFillLayout(true);
}

void SearchEngineChoiceDialogView::ShowNativeView() {
  auto* widget = GetWidget();
  if (!widget) {
    return;
  }

  constrained_window::UpdateWebContentsModalDialogPosition(
      widget, browser_->window()->GetWebContentsModalDialogHost());
  widget->Show();
  web_view_->RequestFocus();
}

void SearchEngineChoiceDialogView::CloseView() {
  GetWidget()->Close();
}

BEGIN_METADATA(SearchEngineChoiceDialogView, views::View)
END_METADATA
