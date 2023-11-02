// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/assistant_stopped_bubble_coordinator_impl.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/autofill_assistant/password_change/apc_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_stopped_bubble_coordinator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/side_panel_toolbar_button.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr char kUmaKeyAssistantStoppedBubbleCloseReason[] =
    "PasswordManager.AutomaticChange.AssistantStoppedBubbleCloseReason";

}  // namespace

class AssistantStoppedBubbleCoordinatorDelegate
    : public ui::DialogModelDelegate {
 public:
  AssistantStoppedBubbleCoordinatorDelegate(content::WebContents* web_contents,
                                            const GURL& url,
                                            const std::string& username)
      : web_contents_(web_contents), url_(url), username_(username) {}

  AssistantStoppedBubbleCoordinatorDelegate(
      const AssistantStoppedBubbleCoordinatorDelegate&) = delete;
  AssistantStoppedBubbleCoordinatorDelegate& operator=(
      const AssistantStoppedBubbleCoordinatorDelegate&) = delete;

  ~AssistantStoppedBubbleCoordinatorDelegate() override = default;

  void RestartScript() {
    content::OpenURLParams params(
        url_, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
        ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
    web_contents_->OpenURL(params);

    ApcClient* apc_client = ApcClient::GetOrCreateForWebContents(web_contents_);
    apc_client->Start(url_, username_,
                      /*skip_login=*/false,
                      /*callback=*/base::DoNothing());
  }

 private:
  const raw_ptr<content::WebContents> web_contents_;
  const GURL url_;
  const std::string username_;
};

std::unique_ptr<AssistantStoppedBubbleCoordinator>
AssistantStoppedBubbleCoordinator::Create(content::WebContents* web_contents,
                                          const GURL& url,
                                          const std::string& username) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser == nullptr) {
    return nullptr;
  }
  return std::make_unique<AssistantStoppedBubbleCoordinatorImpl>(web_contents,
                                                                 url, username);
}

AssistantStoppedBubbleCoordinatorImpl::AssistantStoppedBubbleCoordinatorImpl(
    content::WebContents* web_contents,
    const GURL& url,
    const std::string& username)
    : content::WebContentsObserver(web_contents),
      url_(url),
      username_(username) {}

AssistantStoppedBubbleCoordinatorImpl::
    ~AssistantStoppedBubbleCoordinatorImpl() {
  if (record_metric_) {
    base::UmaHistogramEnumeration(kUmaKeyAssistantStoppedBubbleCloseReason,
                                  widget_
                                      ? CloseReason::kBubbleClosedImplicitly
                                      : CloseReason::kBubbleClosedExplicitly);
  }

  if (widget_)
    widget_->Close();
}

void AssistantStoppedBubbleCoordinatorImpl::CreateWidget() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  DCHECK(browser);

  // `this` is removed from TabStripModel observers list inside
  // TabStripModelObser implementation. It is removed either during its down
  // destruction or at TabStripModelObserver::ModelDestroyed
  browser->tab_strip_model()->AddObserver(this);

  auto bubble_delegate_unique =
      std::make_unique<AssistantStoppedBubbleCoordinatorDelegate>(
          web_contents(), url_, username_);
  AssistantStoppedBubbleCoordinatorDelegate* bubble_delegate =
      bubble_delegate_unique.get();

  std::unique_ptr<ui::DialogModel> dialog =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique))
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_ASSISTANT_ASSISTANT_STOPPED_BUBBLE_TITLE))
          .DisableCloseOnDeactivate()
          .AddParagraph(ui::DialogModelLabel::CreateWithReplacement(
              IDS_AUTOFILL_ASSISTANT_ASSISTANT_STOPPED_BUBBLE_DESCRIPTION,
              ui::DialogModelLabel::CreateLink(
                  IDS_AUTOFILL_ASSISTANT_ASSISTANT_STOPPED_BUBBLE_TRY_AGAIN,
                  base::BindRepeating(&AssistantStoppedBubbleCoordinatorImpl::
                                          RestartLinkClicked,
                                      base::Unretained(this),
                                      bubble_delegate))))
          .Build();
  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog),
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider()
          ->GetSidePanelButton(),
      views::BubbleBorder::TOP_RIGHT);

  widget_ = views::BubbleDialogDelegate::CreateBubble(std::move(bubble))
                ->GetWeakPtr();
}

void AssistantStoppedBubbleCoordinatorImpl::Show() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  // This could happen if the WebContents is being dragged out of a browser.
  if (!browser) {
    return;
  }

  if (!widget_) {
    CreateWidget();
  }
  if (web_contents()->GetVisibility() == content::Visibility::VISIBLE) {
    record_metric_ = true;
    widget_->Show();
  }
}

void AssistantStoppedBubbleCoordinatorImpl::Hide() {
  if (widget_) {
    widget_->Hide();
  }
}

void AssistantStoppedBubbleCoordinatorImpl::Close() {
  if (widget_) {
    widget_->Close();
    widget_ = nullptr;
  }
}

void AssistantStoppedBubbleCoordinatorImpl::RestartLinkClicked(
    AssistantStoppedBubbleCoordinatorDelegate* bubble_delegate) {
  // Since the metric is recorded here already, no action is needed in the
  // destructor.
  record_metric_ = false;
  base::UmaHistogramEnumeration(kUmaKeyAssistantStoppedBubbleCloseReason,
                                CloseReason::kRestartLinkClicked);
  if (bubble_delegate)
    bubble_delegate->RestartScript();
}

void AssistantStoppedBubbleCoordinatorImpl::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // If the tab gets removed from the browser, close the bubble.
  // This happen either when the tab is closed, or when it's moved
  // to a different browser (window).
  if (widget_ && change.type() == TabStripModelChange::kRemoved) {
    const TabStripModelChange::Remove* remove = change.GetRemove();
    for (const auto& removed_tab : remove->contents) {
      if (removed_tab.contents == web_contents()) {
        widget_->Close();
        return;
      }
    }
  }
}

void AssistantStoppedBubbleCoordinatorImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  if (!widget_) {
    return;
  }
  if (visibility == content::Visibility::HIDDEN) {
    widget_->Hide();
  } else if (visibility == content::Visibility::VISIBLE) {
    record_metric_ = true;
    widget_->Show();
  }
}
