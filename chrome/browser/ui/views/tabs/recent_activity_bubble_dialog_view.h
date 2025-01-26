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
class RecentActivityRowImageView;
class CollaborationMessagingPageActionIconView;

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
  // Close this bubble.
  void Close();

  const GURL url_;

  base::WeakPtrFactory<RecentActivityBubbleDialogView> weak_factory_{this};
};

// View containing a single ActivityLogItem. Each row shows activity
// text, metadata text, and an avatar/favicon view.
class RecentActivityRowView : public views::View {
  METADATA_HEADER(RecentActivityRowView, View)

 public:
  RecentActivityRowView(collaboration::messaging::ActivityLogItem item,
                        Profile* profile,
                        base::OnceCallback<void()> close_callback);
  ~RecentActivityRowView() override;

  // views::Views
  bool OnMousePressed(const ui::MouseEvent& event) override;

  RecentActivityRowImageView* image_view() const { return image_view_; }
  const std::u16string& activity_text() const { return activity_text_; }
  const std::u16string& metadata_text() const { return metadata_text_; }

  // RecentActivityAction handlers.
  // Focuses the open tab in the tab strip.
  void FocusTab();
  // Reopens the tab at the end of the group.
  void ReopenTab();
  // Opens the Tab Group editor bubble for the group.
  void OpenTabGroupEditDialog();
  // Opens the Data Sharing management bubble for the group.
  void ManageSharing();

 private:
  std::u16string activity_text_;
  std::u16string metadata_text_;
  raw_ptr<RecentActivityRowImageView> image_view_ = nullptr;
  collaboration::messaging::ActivityLogItem item_;
  const raw_ptr<Profile> profile_ = nullptr;
  base::OnceCallback<void()> close_callback_;
};

// View containing the avatar image and, if the event refers to a tab, the
// favicon of the tab. This view performs both asynchronous image fetches.
class RecentActivityRowImageView : public views::View {
  METADATA_HEADER(RecentActivityRowImageView, View)

 public:
  RecentActivityRowImageView(collaboration::messaging::ActivityLogItem item,
                             Profile* profile);
  ~RecentActivityRowImageView() override;

  // Returns whether there is an avatar image to show.
  bool ShouldShowAvatar() const { return !avatar_image_.isNull(); }

  // Returns whether there is a favicon image to show.
  bool ShouldShowFavicon() const { return !resized_favicon_image_.isNull(); }

 private:
  // views::View
  void OnPaint(gfx::Canvas* canvas) override;

  // Perform the avatar fetch, calling |SetAvatar| when complete.
  void FetchAvatar();
  void SetAvatar(const gfx::Image& avatar);

  // Perform the favicon fetch, calling |SetFavicon| when complete.
  void FetchFavicon();
  void SetFavicon(const favicon_base::FaviconImageResult& favicon);
  void PaintFavicon(gfx::Canvas* canvas, gfx::Rect avatar_bounds);

  base::CancelableTaskTracker favicon_fetching_task_tracker_;
  gfx::ImageSkia avatar_image_;
  gfx::ImageSkia resized_favicon_image_;
  collaboration::messaging::ActivityLogItem item_;
  const raw_ptr<Profile> profile_ = nullptr;

  base::WeakPtrFactory<RecentActivityRowImageView> weak_factory_{this};
};

// The bubble coordinator for Shared Tab Group Recent Activity.
class RecentActivityBubbleCoordinator : public views::WidgetObserver {
 public:
  RecentActivityBubbleCoordinator();
  ~RecentActivityBubbleCoordinator() override;

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // The RecentActivity dialog is used in multiple places, anchoring to
  // different items. Two public method overloads are supplied here so
  // the correct arrow will be used.
  //
  // Calls ShowCommon with the default arrow.
  void Show(views::View* anchor_view,
            content::WebContents* web_contents,
            std::vector<collaboration::messaging::ActivityLogItem> activity_log,
            Profile* profile);
  // Same as above, but provides a default arrow for anchoring to the
  // page action. The default for location bar bubbles is to have a
  // TOP_RIGHT arrow.
  void Show(CollaborationMessagingPageActionIconView* anchor_view,
            content::WebContents* web_contents,
            std::vector<collaboration::messaging::ActivityLogItem> activity_log,
            Profile* profile);
  void Hide();

  RecentActivityBubbleDialogView* GetBubble() const;
  bool IsShowing();

 private:
  // Show a bubble containing the given activity log.
  void ShowCommon(
      views::View* anchor_view,
      content::WebContents* web_contents,
      std::vector<collaboration::messaging::ActivityLogItem> activity_log,
      Profile* profile,
      views::BubbleBorder::Arrow arrow);

  views::ViewTracker tracker_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_RECENT_ACTIVITY_BUBBLE_DIALOG_VIEW_H_
