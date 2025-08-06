// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_RECENT_ACTIVITY_BUBBLE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_RECENT_ACTIVITY_BUBBLE_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/favicon_base/favicon_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;

class RecentActivityRowView;
class RecentActivityRowImageView;

using collaboration::messaging::ActivityLogItem;

DECLARE_ELEMENT_IDENTIFIER_VALUE(kRecentActivityBubbleDialogId);

// The bubble dialog view housing the Shared Tab Group Recent Activity.
// Shows at most kMaxNumberRows of the activity_log parameter.
class RecentActivityBubbleDialogView : public LocationBarBubbleDelegateView,
                                       public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(RecentActivityBubbleDialogView, LocationBarBubbleDelegateView)

 public:
  enum OptionsMenuItem { SEE_ALL_ACTIVITY };

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCloseButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSeeAllActivityId);

  RecentActivityBubbleDialogView(
      View* anchor_view,
      content::WebContents* web_contents,
      std::vector<ActivityLogItem> tab_activity_log,
      std::vector<ActivityLogItem> group_activity_log,
      Profile* profile);
  ~RecentActivityBubbleDialogView() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // The maximum number of rows that can be displayed in this dialog.
  static constexpr int kMaxNumberRows = 5;

  // Creates a state indicating there is no activity to show.
  void CreateEmptyState();

  // Creates a view containing the single most recent tab activity.
  void CreateTabActivity();

  // Creates a view containing the most recent activity for the group.
  void CreateGroupActivity();

  // Returns the title view container including the title, the menu button, and
  // the close button.
  std::u16string GetTitleForTesting();

  // Returns the row's view at the given index. This will look in both
  // the tab activity container and the group activity container.
  RecentActivityRowView* GetRowForTesting(int n);

  views::View* tab_activity_container() const {
    return tab_activity_container_;
  }
  views::View* group_activity_container() const {
    return group_activity_container_;
  }

 private:
  // View IDs used for selecting views in tests.
  enum RecentActivityViewID {
    TITLE_VIEW_ID,
    TITLE_ID,
  };

  // Close this bubble.
  void Close();

  // Creates a button view for the close button.
  std::unique_ptr<views::Button> CreateCloseButton();

  // Creates a button view for the 3-dot menu button.
  std::unique_ptr<views::Button> CreateOptionsMenuButton();

  // Creates the top row of the dialog, including the title of the dialog, the
  // 3-dot menu button, and the close button.
  void CreateTitleView();

  // Displays a context menu anchored to |source|, allowing users to access
  // additional actions like "See All Activity".
  void ShowOptionsMenu(views::Button* source);

  std::unique_ptr<ui::SimpleMenuModel> options_menu_model_;
  std::unique_ptr<views::MenuRunner> options_menu_runner_;

  // Containers will always be non-null. Visibility is toggled based on
  // whether rows are added to each container.
  raw_ptr<views::View> tab_activity_container_ = nullptr;
  raw_ptr<views::View> group_activity_container_ = nullptr;

  std::vector<ActivityLogItem> tab_activity_log_;
  std::vector<ActivityLogItem> group_activity_log_;
  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<RecentActivityBubbleDialogView> weak_factory_{this};
};

// View containing a single ActivityLogItem. Each row shows activity
// text, metadata text, and an avatar/favicon view.
class RecentActivityRowView : public HoverButton {
  METADATA_HEADER(RecentActivityRowView, View)

 public:
  RecentActivityRowView(ActivityLogItem item,
                        Profile* profile,
                        base::OnceCallback<void()> close_callback);
  ~RecentActivityRowView() override;

  // HoverButton
  void ButtonPressed();

  RecentActivityRowImageView* image_view() const { return image_view_; }

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
  raw_ptr<RecentActivityRowImageView> image_view_ = nullptr;
  ActivityLogItem item_;
  const raw_ptr<Profile> profile_ = nullptr;
  base::OnceCallback<void()> close_callback_;
};

// View containing the avatar image and, if the event refers to a tab, the
// favicon of the tab. This view performs both asynchronous image fetches.
class RecentActivityRowImageView : public views::View {
  METADATA_HEADER(RecentActivityRowImageView, View)

 public:
  RecentActivityRowImageView(ActivityLogItem item, Profile* profile);
  ~RecentActivityRowImageView() override;

  // Returns whether there is an avatar image to show.
  bool ShouldShowAvatar() const { return avatar_request_complete_; }

  // Returns whether there is a favicon image to show.
  bool ShouldShowFavicon() const { return !resized_favicon_image_.isNull(); }

 private:
  // views::View
  void OnPaint(gfx::Canvas* canvas) override;

  // Perform the avatar fetch, calling `SetAvatar` when complete.
  void FetchAvatar();
  void SetAvatar(const gfx::Image& avatar);

  // Perform the favicon fetch, calling `SetFavicon` when complete.
  void FetchFavicon();
  void SetFavicon(const favicon_base::FaviconImageResult& favicon);
  void PaintFavicon(gfx::Canvas* canvas, const gfx::Rect& avatar_bounds);
  void PaintPlaceholderBackground(gfx::Canvas* canvas, const gfx::Rect& bounds);
  void PaintFallbackIcon(gfx::Canvas* canvas, const gfx::Rect& bounds);

  // When the avatar request is complete (or there is no avatar to
  // request), this will be set to true. While the value is false, we
  // will paint the background color as a placeholder for the avatar.
  bool avatar_request_complete_ = false;

  base::CancelableTaskTracker favicon_fetching_task_tracker_;
  gfx::ImageSkia avatar_image_;
  gfx::ImageSkia resized_favicon_image_;
  ActivityLogItem item_;
  const raw_ptr<Profile> profile_ = nullptr;

  base::WeakPtrFactory<RecentActivityRowImageView> weak_factory_{this};
};

// The bubble coordinator for Shared Tab Group Recent Activity.
class RecentActivityBubbleCoordinator : public views::WidgetObserver {
 public:
  explicit RecentActivityBubbleCoordinator(BrowserWindowInterface* browser);
  ~RecentActivityBubbleCoordinator() override;

  DECLARE_USER_DATA(RecentActivityBubbleCoordinator);
  static RecentActivityBubbleCoordinator* From(BrowserWindowInterface* browser);

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // The RecentActivity dialog is used in multiple places, anchoring to
  // different items. Two public method overloads are supplied here so
  // the correct arrow will be used.
  //
  // Calls ShowCommon with the default arrow.
  void Show(views::View* anchor_view,
            content::WebContents* web_contents,
            std::vector<ActivityLogItem> activity_log,
            Profile* profile);
  // Same as above, but provides a default arrow for anchoring to the
  // page action. The default for location bar bubbles is to have a
  // TOP_RIGHT arrow.
  void ShowForCurrentTab(views::View* anchor_view,
                         content::WebContents* web_contents,
                         std::vector<ActivityLogItem> tab_activity_log,
                         std::vector<ActivityLogItem> group_activity_log,
                         Profile* profile);
  void Hide();

  RecentActivityBubbleDialogView* GetBubble() const;
  bool IsShowing();

 private:
  // Show a bubble containing the given activity log.
  void ShowCommon(std::unique_ptr<RecentActivityBubbleDialogView> bubble);

  views::ViewTracker tracker_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};
  ui::ScopedUnownedUserData<RecentActivityBubbleCoordinator>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_RECENT_ACTIVITY_BUBBLE_DIALOG_VIEW_H_
