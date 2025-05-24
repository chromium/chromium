// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/capture_mode/search_results_view.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/capture_mode/capture_mode_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/web_view/ash_web_view_impl.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/window_open_disposition.h"

namespace ash {

namespace {

// The rounded corners for the web view.
constexpr gfx::RoundedCornersF kRoundedCorners(16);

AshWebView::InitParams GetInitParams() {
  AshWebView::InitParams params;
  params.suppress_navigation = true;
  params.rounded_corners = kRoundedCorners;
  return params;
}

// Modifies `new_tab_params` to open in a new tab.
void OpenURLFromTabInternal(NavigateParams& new_tab_params) {
  new_tab_params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&new_tab_params);
}

}  // namespace

SearchResultsView::SearchResultsView() : AshWebViewImpl(GetInitParams()) {
  // We should not use `CanShowSunfishUi` here, as that could change between
  // sending the region and receiving a URL which will then create this view
  // (for example, if the Sunfish policy changes).
  DCHECK(features::IsSunfishFeatureEnabled());
}

SearchResultsView::~SearchResultsView() = default;

content::WebContents* SearchResultsView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  // Open the URL specified by `params` in a new tab.
  NavigateParams new_tab_params(static_cast<Browser*>(nullptr), params.url,
                                params.transition);
  switch (params.disposition) {
    case WindowOpenDisposition::UNKNOWN:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
    case WindowOpenDisposition::CURRENT_TAB:
    case WindowOpenDisposition::SINGLETON_TAB:
      new_tab_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
      break;
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_POPUP:
    case WindowOpenDisposition::NEW_WINDOW:
    case WindowOpenDisposition::SAVE_TO_DISK:
    case WindowOpenDisposition::OFF_THE_RECORD:
    case WindowOpenDisposition::IGNORE_ACTION:
    case WindowOpenDisposition::SWITCH_TO_TAB:
    case WindowOpenDisposition::NEW_PICTURE_IN_PICTURE:
      // These other dispositions will open new windows / tabs, so use these
      // dispositions as-is.
      new_tab_params.disposition = params.disposition;
      break;
  }
  new_tab_params.initiating_profile =
      Profile::FromBrowserContext(source->GetBrowserContext());
  OpenURLFromTabInternal(new_tab_params);

  if (auto* controller = CaptureModeController::Get()) {
    controller->OnSearchResultClicked();
  }
  return new_tab_params.navigated_or_inserted_contents;
}

bool SearchResultsView::IsWebContentsCreationOverridden(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  // All navigation attempts are suppressed by `suppress_navigation`, which is
  // needed to override opening links in `OpenURLFromTab()`. Ensure new web
  // contents also open in new tabs. See
  // `AshWebViewImpl::NotifyDidSuppressNavigation()`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::WeakPtr<SearchResultsView>& self, GURL url) {
            if (self) {
              self->OpenURLFromTab(
                  self->web_contents(),
                  content::OpenURLParams(
                      url, content::Referrer(),
                      WindowOpenDisposition::NEW_FOREGROUND_TAB,
                      ui::PAGE_TRANSITION_LINK,
                      /*is_renderer_initiated=*/false),
                  /*navigation_handle_callback=*/{});
            }
          },
          weak_factory_.GetWeakPtr(), target_url));
  return false;
}

bool SearchResultsView::TakeFocus(content::WebContents* web_contents,
                                  bool reverse) {
  // If we are in a capture session, we need the `CaptureModeSessionFocusCycler`
  // to handle focus traversal differently.
  auto* controller = CaptureModeController::Get();
  if (controller->IsActive()) {
    return controller->capture_mode_session()->TakeFocusForSearchResultsPanel(
        reverse);
  }

  return AshWebViewImpl::TakeFocus(web_contents, reverse);
}

void SearchResultsView::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  // Make sure we aren't pseudo focusing another capture session item when the
  // web contents take focus.
  auto* controller = CaptureModeController::Get();
  if (controller->IsActive()) {
    controller->capture_mode_session()->ClearPseudoFocus();
    // The session focus cycler may have set a different a11y override window
    // when the web contents take focus, so we'll need to set it back to the
    // panel for screenreaders like ChromeVox to work properly.
    controller->capture_mode_session()
        ->SetA11yOverrideWindowToSearchResultsPanel();
  }

  AshWebViewImpl::OnWebContentsFocused(render_widget_host);
}

BEGIN_METADATA(SearchResultsView)
END_METADATA

}  // namespace ash
