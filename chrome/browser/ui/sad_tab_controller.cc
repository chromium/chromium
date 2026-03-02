// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sad_tab_controller.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {

ContentsWebView* FindContentsWebView(content::WebContents* web_contents) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return nullptr;
  }

  // In unit tests, browser->GetWindow() might not be a real BrowserView.
  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  if (!browser || !browser->GetWindow() ||
      !browser->GetWindow()->GetNativeWindow()) {
    return nullptr;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  DCHECK(browser_view);

  for (auto* contents_view : browser_view->GetAllVisibleContentsWebViews()) {
    if (contents_view->web_contents() == web_contents) {
      return contents_view;
    }
  }

  return nullptr;
}

}  // namespace

SadTabController::SadTabController(content::WebContents* web_contents,
                                   SadTabKind kind)
    : SadTab(web_contents, kind) {
  ReinstallInWebView();
}

SadTabController::~SadTabController() {
  ContentsWebView* contents_view = FindContentsWebView(web_contents());
  if (!contents_view) {
    return;
  }

  if (view_tracker_ && !owned_sad_tab_view_) {
    auto crashed_overlay_view = contents_view->DetachCrashedOverlayView();
    CHECK_EQ(view_tracker_.view(), crashed_overlay_view.get());
  }
}

void SadTabController::ReinstallInWebView() {
  std::unique_ptr<SadTabView> sad_tab_view;
  if (contents_view_tracker_) {
    ContentsWebView* contents_view =
        views::AsViewClass<ContentsWebView>(contents_view_tracker_.view());
    if (view_tracker_) {
      sad_tab_view =
          owned_sad_tab_view_
              ? std::move(owned_sad_tab_view_)
              : contents_view->DetachCrashedOverlayView<SadTabView>();
    }
    contents_view_tracker_.SetView(nullptr);
  }

  ContentsWebView* contents_view = FindContentsWebView(web_contents());
  if (!contents_view) {
    return;
  }

  if (!sad_tab_view) {
    sad_tab_view = std::make_unique<SadTabView>(
        this, kind(), GetTitle(), GetInfoMessage(), GetSubMessages(),
        GetErrorCodeFormatString(), GetCrashedErrorCode(), GetButtonTitle(),
        GetHelpLinkTitle());
    view_tracker_.SetView(sad_tab_view.get());
  }
  CHECK_EQ(view_tracker_.view(), sad_tab_view.get());
  sad_tab_view->SetBackgroundRadii(contents_view->GetBackgroundRadii());
  contents_view->TakeCrashedOverlayView(
      std::move(sad_tab_view),
      base::BindOnce(
          [](base::WeakPtr<SadTabController> controller,
             std::unique_ptr<views::View> crashed_overlay_view) {
            CHECK(!controller->owned_sad_tab_view_);
            std::unique_ptr<SadTabView> sad_tab_view =
                views::AsViewClass<SadTabView>(std::move(crashed_overlay_view));
            CHECK(sad_tab_view);
            controller->owned_sad_tab_view_.swap(sad_tab_view);
          },
          weak_factory_.GetWeakPtr()));
  contents_view_tracker_.SetView(contents_view);
}

void SadTabController::SetBackgroundRadii(const gfx::RoundedCornersF& radii) {
  if (view_tracker_.view()) {
    views::AsViewClass<SadTabView>(view_tracker_.view())
        ->SetBackgroundRadii(radii);
  }
}

gfx::RoundedCornersF SadTabController::GetBackgroundRadii() const {
  if (view_tracker_) {
    return views::AsViewClass<SadTabView>(view_tracker_.view())
        ->GetBackgroundRadii();
  }
  return gfx::RoundedCornersF();
}

void SadTabController::RequestFocus() {
  if (view_tracker_) {
    view_tracker_.view()->RequestFocus();
  }
}

// SadTab::Create implementation moved here.
std::unique_ptr<SadTab> SadTab::Create(content::WebContents* web_contents,
                                       SadTabKind kind) {
  return std::make_unique<SadTabController>(web_contents, kind);
}
