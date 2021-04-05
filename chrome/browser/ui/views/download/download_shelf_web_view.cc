// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_shelf_web_view.h"

#include <memory>

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/gfx/animation/animation.h"
#include "ui/views/border.h"

DownloadShelfWebView::DownloadShelfWebView(Browser* browser,
                                           BrowserView* parent)
    : DownloadShelf(browser, browser->profile()),
      WebView(browser->profile()),
      AnimationDelegateViews(this),
      parent_(parent) {
  SetVisible(false);

  LoadInitialURL(GURL(chrome::kChromeUIDownloadShelfURL));

  shelf_animation_.SetSlideDuration(base::TimeDelta::FromMilliseconds(
      gfx::Animation::ShouldRenderRichAnimation() ? 120 : 0));

  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents());
  task_manager::WebContentsTags::CreateForTabContents(web_contents());

  DownloadShelfUI* download_shelf_ui = GetDownloadShelfUI();
  if (download_shelf_ui)
    download_shelf_ui->set_embedder(this);
}

DownloadShelfWebView::~DownloadShelfWebView() = default;

gfx::Size DownloadShelfWebView::CalculatePreferredSize() const {
  return gfx::Tween::SizeValueBetween(shelf_animation_.GetCurrentValue(),
                                      gfx::Size(), gfx::Size(0, 50));
}

void DownloadShelfWebView::OnThemeChanged() {
  views::WebView::OnThemeChanged();
  SetBorder(views::CreateSolidSidedBorder(
      1, 0, 0, 0,
      GetThemeProvider()->GetColor(
          ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR)));
}

void DownloadShelfWebView::DoShowDownload(
    DownloadUIModel::DownloadUIModelPtr download) {
  DownloadShelfUI* download_shelf_ui = GetDownloadShelfUI();
  if (download_shelf_ui) {
    download_shelf_ui->DoShowDownload(std::move(download));
  }
}

void DownloadShelfWebView::DoOpen() {
  SetVisible(true);
  shelf_animation_.Show();
}

void DownloadShelfWebView::DoClose() {
  parent_->SetDownloadShelfVisible(false);
  shelf_animation_.Hide();
}

void DownloadShelfWebView::DoHide() {
  SetVisible(false);
  parent_->SetDownloadShelfVisible(false);
  parent_->ToolbarSizeChanged(false);
}

void DownloadShelfWebView::DoUnhide() {
  SetVisible(true);
  parent_->ToolbarSizeChanged(true);
  parent_->SetDownloadShelfVisible(true);
}

void DownloadShelfWebView::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(&shelf_animation_, animation);
  // Force a re-layout of the parent, which will call back into
  // GetPreferredSize(), where we will do our animation. In the case where the
  // animation is hiding, we do a full resize - the fast resizing would
  // otherwise leave blank white areas where the shelf was and where the
  // user's eye is. Thankfully bottom-resizing is a lot faster than
  // top-resizing.
  parent_->ToolbarSizeChanged(shelf_animation_.IsShowing());
}

void DownloadShelfWebView::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(&shelf_animation_, animation);
  const bool shown = shelf_animation_.IsShowing();
  parent_->SetDownloadShelfVisible(shown);
}

views::View* DownloadShelfWebView::GetView() {
  return this;
}

void DownloadShelfWebView::ShowDownloadContextMenu(DownloadUIModel* download,
                                                   const gfx::Point& position) {
  gfx::Point screen_position = position;
  ConvertPointToScreen(this, &screen_position);
  context_menu_view_ = std::make_unique<DownloadShelfContextMenuView>(download);
  context_menu_view_->Run(
      GetWidget(), gfx::Rect(screen_position, gfx::Size()),
      /* TODO(kerenzhu): Investigate if we need other MenuSourceTypes. */
      ui::MenuSourceType::MENU_SOURCE_MOUSE, base::RepeatingClosure());
}

bool DownloadShelfWebView::IsShowing() const {
  return GetVisible() && shelf_animation_.IsShowing();
}

bool DownloadShelfWebView::IsClosing() const {
  return shelf_animation_.IsClosing();
}

DownloadShelfUI* DownloadShelfWebView::GetDownloadShelfUI() {
  content::WebUI* web_ui = GetWebContents()->GetWebUI();
  return web_ui ? web_ui->GetController()->GetAs<DownloadShelfUI>() : nullptr;
}
