// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_STOPPED_BUBBLE_COORDINATOR_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_STOPPED_BUBBLE_COORDINATOR_IMPL_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_stopped_bubble_coordinator.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Widget;
}  // namespace views

class AssistantStoppedBubbleCoordinatorDelegate;

class AssistantStoppedBubbleCoordinatorImpl
    : public AssistantStoppedBubbleCoordinator,
      public content::WebContentsObserver,
      public TabStripModelObserver {
 public:
  // The reasons for why the bubble was closed that are logged as UMA metrics.
  // These values are used in `enums.xml`; do not reorder or renumber entries!
  enum class CloseReason {
    // The user clicked the link to restart the flow.
    kRestartLinkClicked = 0,
    // The user clicked the close icon on the bubble.
    kBubbleClosedExplicitly = 1,
    // The user either moved the tab, closed the tab, or closed the browser
    // while the bubble was still showing.
    kBubbleClosedImplicitly = 2,

    kMaxValue = kBubbleClosedImplicitly
  };

  AssistantStoppedBubbleCoordinatorImpl(content::WebContents* web_contents,
                                        const GURL& url,
                                        const std::string& username);

  AssistantStoppedBubbleCoordinatorImpl(
      const AssistantStoppedBubbleCoordinatorImpl&) = delete;
  AssistantStoppedBubbleCoordinatorImpl& operator=(
      const AssistantStoppedBubbleCoordinatorImpl&) = delete;
  ~AssistantStoppedBubbleCoordinatorImpl() override;

  void Show() override;
  void Hide() override;
  void Close() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AssistantStoppedBubbleCoordinatorImplTest,
                           RecordsMetricOnRestartLinkClick);

  void RestartLinkClicked(
      AssistantStoppedBubbleCoordinatorDelegate* bubble_delegate);

  void CreateWidget();

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;

  // content::TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  base::WeakPtr<views::Widget> widget_ = nullptr;
  const GURL url_;
  const std::string username_;
  // Indicates whether it is still necessary to record a metric about the
  // bubble's behavior, e.g. on destruction of the coordinator.
  bool record_metric_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_STOPPED_BUBBLE_COORDINATOR_IMPL_H_
