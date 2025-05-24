// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble_context_views.h"

#include <memory>

#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble_session_observer_views.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/views/caption_bubble.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/widget/widget.h"

namespace captions {

// Static
std::unique_ptr<CaptionBubbleContextBrowser>
CaptionBubbleContextBrowser::Create(content::WebContents* web_contents) {
  return std::make_unique<CaptionBubbleContextViews>(web_contents);
}

CaptionBubbleContextViews::CaptionBubbleContextViews(
    content::WebContents* web_contents)
    : CaptionBubbleContextBrowser(web_contents),
      web_contents_(web_contents),
      web_contents_observer_(
          std::make_unique<CaptionBubbleSessionObserverViews>(web_contents)) {
  auto* browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser || !browser->is_type_normal()) {
    return;
  }
  browser->tab_strip_model()->AddObserver(this);
}

CaptionBubbleContextViews::~CaptionBubbleContextViews() {
  caption_bubble_ = nullptr;
  auto* browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser || !browser->is_type_normal()) {
    return;
  }
  browser->tab_strip_model()->RemoveObserver(this);
}

void CaptionBubbleContextViews::GetBounds(GetBoundsCallback callback) const {
  if (!web_contents_) {
    return;
  }

  views::Widget* context_widget = views::Widget::GetTopLevelWidgetForNativeView(
      web_contents_->GetNativeView());
  if (!context_widget) {
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                context_widget->GetClientAreaBoundsInScreen()));
}

const std::string CaptionBubbleContextViews::GetSessionId() const {
  return web_contents_->GetBrowserContext()->UniqueId();
}

void CaptionBubbleContextViews::Activate() {
  if (!web_contents_) {
    return;
  }
  // Activate the web contents and the browser window that the web contents is
  // in. Order matters: web contents needs to be active in order for the widget
  // getter to work.
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser) {
    return;
  }
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  if (!tab_strip_model) {
    return;
  }
  int index = tab_strip_model->GetIndexOfWebContents(web_contents_);
  if (index == TabStripModel::kNoTab) {
    return;
  }
  tab_strip_model->ActivateTabAt(index);
  views::Widget* context_widget = views::Widget::GetTopLevelWidgetForNativeView(
      web_contents_->GetNativeView());
  if (context_widget) {
    context_widget->Activate();
  }
}

bool CaptionBubbleContextViews::IsActivatable() const {
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser) {
    return false;
  }
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  if (!tab_strip_model) {
    return false;
  }

  return tab_strip_model->GetIndexOfWebContents(web_contents_) !=
         tab_strip_model->active_index();
}

bool CaptionBubbleContextViews::ShouldAvoidOverlap() const {
  // Avoid overlap if web_contents_ doesn't belong to a tab.
  return !chrome::FindBrowserWithTab(web_contents_);
}

std::unique_ptr<CaptionBubbleSessionObserver>
CaptionBubbleContextViews::GetCaptionBubbleSessionObserver() {
  if (web_contents_observer_) {
    return std::move(web_contents_observer_);
  }

  return nullptr;
}

void CaptionBubbleContextViews::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (caption_bubble_) {
    caption_bubble_->OnContextActivatabilityChanged();
  }
}

OpenCaptionSettingsCallback
CaptionBubbleContextViews::GetOpenCaptionSettingsCallback() {
  // Unretained is safe because the caption bubble context outlives the caption
  // bubble that uses this callback.
  return base::BindRepeating(&CaptionBubbleContextViews::OpenCaptionSettings,
                             base::Unretained(this));
}

void CaptionBubbleContextViews::SetContextActivatabilityObserver(
    CaptionBubble* caption_bubble) {
  caption_bubble_ = caption_bubble;
}

void CaptionBubbleContextViews::RemoveContextActivatabilityObserver() {
  caption_bubble_ = nullptr;
}

void CaptionBubbleContextViews::OpenCaptionSettings() {
  content::OpenURLParams params(GURL(GetCaptionSettingsUrl()),
                                content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params, /*navigation_handle_callback=*/{});
}
}  // namespace captions
