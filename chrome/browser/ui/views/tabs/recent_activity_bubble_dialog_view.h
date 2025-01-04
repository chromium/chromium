// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_RECENT_ACTIVITY_BUBBLE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_RECENT_ACTIVITY_BUBBLE_DIALOG_VIEW_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/favicon_base/favicon_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class WebContents;
}  // namespace content

class GURL;
class Profile;

class RecentActivityRowView;

DECLARE_ELEMENT_IDENTIFIER_VALUE(kRecentActivityBubbleDialogId);

// The bubble dialog view housing the Shared Tab Group Recent Activity.
// Shows at most kMaxNumberRows of the activity_log parameter.
class RecentActivityBubbleDialogView : public LocationBarBubbleDelegateView {
  METADATA_HEADER(RecentActivityBubbleDialogView, LocationBarBubbleDelegateView)

 public:
  RecentActivityBubbleDialogView(
      View* anchor_view,
      content::WebContents* web_contents,
      std::vector<collaboration::messaging::ActivityLogItem> activity_log,
      Profile* profile);
  ~RecentActivityBubbleDialogView() override;

  // The maximum number of rows that can be displayed in this dialog.
  static constexpr int kMaxNumberRows = 5;

  // Returns the row's view at the given index.
  RecentActivityRowView* GetRowForTesting(int n);

 private:
  const GURL url_;

  base::WeakPtrFactory<RecentActivityBubbleDialogView> weak_factory_{this};
};

// View containing a single ActivityLogItem. Each row shows activity
// text, metadata text, and an avatar/favicon view.
class RecentActivityRowView : public views::View {
  METADATA_HEADER(RecentActivityRowView, View)

 public:
  RecentActivityRowView(collaboration::messaging::ActivityLogItem item,
                        Profile* profile);
  ~RecentActivityRowView() override;

  const std::u16string& activity_text() const { return activity_text_; }

  const std::u16string& metadata_text() const { return metadata_text_; }

 private:
  std::u16string activity_text_;
  std::u16string metadata_text_;
};

// View containing the avatar image and, if the event refers to a tab, the
// favicon of the tab. This view performs both asynchronous image fetches.
class RecentActivityRowImageView : public views::View {
  METADATA_HEADER(RecentActivityRowImageView, View)

 public:
  RecentActivityRowImageView(collaboration::messaging::ActivityLogItem item,
                             Profile* profile);
  ~RecentActivityRowImageView() override;

 private:
  // Perform the avatar fetch, calling |SetAvatar| when complete.
  void FetchAvatar();
  void SetAvatar(const gfx::Image& avatar);

  raw_ptr<views::ImageView> avatar_image_ = nullptr;
  base::CancelableTaskTracker cancelable_task_tracker_;

  collaboration::messaging::ActivityLogItem item_;
  const raw_ptr<Profile> profile_ = nullptr;
};

// The bubble coordinator for Shared Tab Group Recent Activity.
class RecentActivityBubbleCoordinator : public views::WidgetObserver {
 public:
  RecentActivityBubbleCoordinator();
  ~RecentActivityBubbleCoordinator() override;

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Show a bubble containing the given activity log.
  void Show(views::View* anchor_view,
            content::WebContents* web_contents,
            std::vector<collaboration::messaging::ActivityLogItem> activity_log,
            Profile* profile);
  void Hide();

  RecentActivityBubbleDialogView* GetBubble() const;
  bool IsShowing();

 private:
  views::ViewTracker tracker_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_RECENT_ACTIVITY_BUBBLE_DIALOG_VIEW_H_
