// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_shelf_web_view.h"

#include <memory>

#include "base/callback.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/base/theme_provider.h"
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

  shelf_animation_.SetSlideDuration(base::Milliseconds(
      gfx::Animation::ShouldRenderRichAnimation() ? 120 : 0));

  task_manager::WebContentsTags::CreateForTabContents(web_contents());

  DownloadShelfUI* download_shelf_ui = GetDownloadShelfUI();
  if (download_shelf_ui)
    download_shelf_ui->set_embedder(this);
}

DownloadShelfWebView::~DownloadShelfWebView() = default;

gfx::Size DownloadShelfWebView::CalculatePreferredSize() const {
  return gfx::Tween::SizeValueBetween(shelf_animation_.GetCurrentValue(),
                                      gfx::Size(), gfx::Size(0, 58));
}

bool DownloadShelfWebView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Suppress native context menu, since the web content shows one.
  return true;
}

void DownloadShelfWebView::OnThemeChanged() {
  views::WebView::OnThemeChanged();
  SetBorder(views::CreateSolidSidedBorder(
      gfx::Insets::TLBR(1, 0, 0, 0),
      GetThemeProvider()->GetColor(
          ThemeProperties::COLOR_DOWNLOAD_SHELF_CONTENT_AREA_SEPARATOR)));
}

void DownloadShelfWebView::DoShowDownload(
    DownloadUIModel::DownloadUIModelPtr download) {
  const base::Time show_download_start_time = base::Time::Now();
  DownloadShelfUI* download_shelf_ui = GetDownloadShelfUI();
  if (download_shelf_ui) {
    download_shelf_ui->DoShowDownload(std::move(download),
                                      show_download_start_time);
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

  if (shown || is_hidden())
    return;

  // The shelf is explicitly closed by the user. First, remove the
  // context menu before DownloadUIModels are removed from DownloadShelfUI.
  context_menu_view_.reset();
  // Then, remove all downloads that are not in progress.
  DownloadShelfUI* download_shelf_ui = GetDownloadShelfUI();
  if (download_shelf_ui) {
    for (DownloadUIModel* model : download_shelf_ui->GetDownloads()) {
      // Treat the item as opened when the shelf closes. This way if it gets
      // shown again the user need not open the item for the shelf to
      // auto-close.
      if ((model->GetState() == download::DownloadItem::IN_PROGRESS) ||
          model->IsDangerous()) {
        model->SetOpened(true);
      } else {
        download_shelf_ui->RemoveDownload(model->download()->GetId());
      }
    }
  }
}

views::View* DownloadShelfWebView::GetView() {
  return this;
}

void DownloadShelfWebView::DoShowAll() {
  chrome::ShowDownloads(browser());
}

void DownloadShelfWebView::ShowDownloadContextMenu(
    DownloadUIModel* download,
    const gfx::Point& position,
    base::OnceClosure on_menu_will_show_callback) {
  gfx::Point screen_position = position;
  ConvertPointToScreen(this, &screen_position);
  context_menu_view_ =
      std::make_unique<DownloadShelfContextMenuView>(download->GetWeakPtr());
  context_menu_view_->SetOnMenuWillShowCallback(
      std::move(on_menu_will_show_callback));
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
