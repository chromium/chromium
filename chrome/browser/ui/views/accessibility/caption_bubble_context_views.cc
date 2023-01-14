// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble_context_views.h"

#include <memory>

#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble_session_observer_views.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
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
          std::make_unique<CaptionBubbleSessionObserverViews>(web_contents)) {}

CaptionBubbleContextViews::~CaptionBubbleContextViews() = default;

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
  if (!web_contents_)
    return;
  // Activate the web contents and the browser window that the web contents is
  // in. Order matters: web contents needs to be active in order for the widget
  // getter to work.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (!browser)
    return;
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  if (!tab_strip_model)
    return;
  int index = tab_strip_model->GetIndexOfWebContents(web_contents_);
  if (index == TabStripModel::kNoTab)
    return;
  tab_strip_model->ActivateTabAt(index);
  views::Widget* context_widget = views::Widget::GetTopLevelWidgetForNativeView(
      web_contents_->GetNativeView());
  if (context_widget)
    context_widget->Activate();
}

bool CaptionBubbleContextViews::IsActivatable() const {
  return true;
}

std::unique_ptr<CaptionBubbleSessionObserver>
CaptionBubbleContextViews::GetCaptionBubbleSessionObserver() {
  if (web_contents_observer_)
    return std::move(web_contents_observer_);

  return nullptr;
}
}  // namespace captions
