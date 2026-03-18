// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_menu_utils.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_tabs_menu_model.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_controls_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_recent_threads_expand_button.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_recent_threads_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_utils.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_view_layout.h"
#include "chrome/browser/ui/views/tabs/shared/rounded_scroll_bar.h"
#include "chrome/browser/ui/views/tabs/vertical/top_container_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/event_monitor.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_shadow.h"
#include "ui/views/widget/widget.h"

namespace {

enum ThreadsActivityMenuCommandId {
  kGeminiActivity = 1,
  kAiModeActivity = 2,
};

constexpr int kClipRectMarginForShadow = 32;
constexpr int kProjectPanelRightCornerRadius = 16;
constexpr int kShadowElevation = 2;
constexpr gfx::Insets kListHeaderMargins = gfx::Insets::TLBR(
    8,
    8 + projects_panel::kProjectsPanelRegionInteriorMargins.left(),
    8,
    8 + projects_panel::kProjectsPanelRegionInteriorMargins.right());
constexpr int kListHeaderHeight = 28;
constexpr int kCreateNewTabGroupIconSize = 20;
constexpr gfx::Insets kCreateNewTabGroupIconMargins =
    gfx::Insets::TLBR(0, 2, 0, 2);

// Border insets applied to the tab groups and threads list to reserve space for
// their scroll bar.
constexpr gfx::Insets kListsInsideBorderInsets = gfx::Insets::TLBR(
    0,
    0,
    0,
    projects_panel::kProjectsPanelRegionInteriorMargins.right());

// Insets containing only the horizontal margins of the panel region. Used by
// the ProjectsPanelNewTabGroupButton and ProjectsPanelRecentThreadsExpandButton
// to account for their containers taking the full width of the panel.
constexpr gfx::Insets kProjectsPanelRegionHorizontalMargins = gfx::Insets::TLBR(
    0,
    projects_panel::kProjectsPanelRegionInteriorMargins.left(),
    0,
    projects_panel::kProjectsPanelRegionInteriorMargins.right());

constexpr base::TimeDelta kPanelShowAnimationDuration = base::Milliseconds(250);
constexpr base::TimeDelta kPanelHideAnimationDuration = base::Milliseconds(200);

constexpr int kThreadsActivityMenuButtonIconSize = 18;
constexpr gfx::Size kThreadsActivityMenuButtonSize =
    gfx::Size(kListHeaderHeight, kListHeaderHeight);
constexpr int kThreadsActivityMenuIconSize = 16;

// Whether the threads section should be visible even if no threads exist. This
// setting is applied the next time the panel is opened.
static bool show_threads_for_testing_ = false;

static bool disable_animations_for_testing_ = false;

class ProjectsPanelNewTabGroupButton : public views::Button {
  METADATA_HEADER(ProjectsPanelNewTabGroupButton, views::Button)

 public:
  explicit ProjectsPanelNewTabGroupButton(base::RepeatingClosure callback)
      : views::Button(std::move(callback)) {
    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetInteriorMargin(projects_panel::kListItemMargins)
        .SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

    auto* icon = AddChildView(std::make_unique<views::ImageView>());
    icon->SetCanProcessEventsWithinSubtree(false);
    icon->SetProperty(views::kMarginsKey, kCreateNewTabGroupIconMargins);
    icon->SetImage(ui::ImageModel::FromVectorIcon(kCreateNewTabGroupIcon,
                                                  kColorProjectsPanelButtonIcon,
                                                  kCreateNewTabGroupIconSize));

    auto* title = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_CREATE_NEW_TAB_GROUP)));
    title->SetTextStyle(views::style::STYLE_BODY_3);
    title->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    title->SetBackgroundColor(SK_ColorTRANSPARENT);
    title->SetProperty(views::kMarginsKey,
                       projects_panel::kListItemTitleMargins);
    title->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                 views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kUnbounded));

    // This view is inside the tab groups container view, which is given the
    // full width of the panel to account for its scroll bar. These margins are
    // applied to properly distance it from the edges of the panel.
    SetProperty(views::kMarginsKey, kProjectsPanelRegionHorizontalMargins);

    projects_panel::ConfigureInkDropForButton(this);
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_CREATE_NEW_TAB_GROUP));
  }
  ProjectsPanelNewTabGroupButton(const ProjectsPanelNewTabGroupButton&) =
      delete;
  ProjectsPanelNewTabGroupButton& operator=(
      const ProjectsPanelNewTabGroupButton&) = delete;
  ~ProjectsPanelNewTabGroupButton() override = default;
};

BEGIN_METADATA(ProjectsPanelNewTabGroupButton)
END_METADATA

// Assigns shared list title properties.
void SetListTitleProperties(views::Label& label) {
  label.SetTextStyle(views::style::TextStyle::STYLE_HEADLINE_5);
  label.SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
}

void SetListTitleContainerProperties(views::View& list_title_container) {
  list_title_container.SetProperty(views::kMarginsKey, kListHeaderMargins);
  list_title_container.SetPreferredSize(gfx::Size(0, kListHeaderHeight));
}

void SetScrollViewProperties(views::ScrollView& scroll_view) {
  scroll_view.SetBackgroundColor(projects_panel::kProjectsPanelBackgroundColor);
  scroll_view.SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view.SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kEnabled);
  scroll_view.SetVerticalScrollBar(std::make_unique<tabs::RoundedScrollBar>());
  scroll_view.SetOverflowGradientMask(
      views::ScrollView::GradientDirection::kVertical);
  scroll_view.SetUseContentsPreferredSize(true);
  // The tab groups and threads containers are given the full width of the panel
  // to account for their scroll bars. The left panel margin is applied here
  // while the right margin is applied to the contents view, so the scroll bar
  // appears beside the content instead of overlapping.
  scroll_view.SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, projects_panel::kProjectsPanelRegionInteriorMargins.left(), 0, 0));
  scroll_view.SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
}

class STGTabsMenuModelWithCallback : public tab_groups::STGTabsMenuModel {
 public:
  STGTabsMenuModelWithCallback(BrowserWindowInterface* browser,
                               tab_groups::TabGroupMenuContext context,
                               base::RepeatingClosure callback)
      : tab_groups::STGTabsMenuModel(browser->GetBrowserForMigrationOnly(),
                                     context),
        callback_(std::move(callback)) {}

  void ExecuteCommand(int command_id, int event_flags) override {
    tab_groups::STGTabsMenuModel::ExecuteCommand(command_id, event_flags);
    callback_.Run();
  }

 private:
  base::RepeatingClosure callback_;
};
}  // namespace

ProjectsPanelView::ProjectsPanelView(
    BrowserWindowInterface* browser,
    actions::ActionItem* root_action_item,
    ProjectsPanelStateController* state_controller)
    : browser_(browser),
      root_action_item_(root_action_item),
      action_view_controller_(std::make_unique<views::ActionViewController>()),
      state_controller_(state_controller),
      resize_animation_(this),
      focus_search_(std::make_unique<views::FocusSearch>(this,
                                                         /*cycle=*/true,
                                                         /*accessibility_mode=*/
                                                         true)) {
  // The vertical tab strip contains ScrollViews that paint to a layer. This
  // view must also paint to a layer to ensure it overlays those components.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  content_container_ = AddChildView(std::make_unique<views::View>());
  content_container_->SetPaintToLayer();
  content_container_->layer()->SetFillsBoundsOpaquely(false);

  content_shadow_ =
      std::make_unique<views::ViewShadow>(content_container_, kShadowElevation);
  content_shadow_->SetRoundedCornerRadius(kProjectPanelRightCornerRadius);

  // Apply the elevated state by default.
  SetIsElevated(true);

  bool threads_enabled = tab_groups::IsThreadsInProjectsPanelEnabled();
  panel_controller_ = std::make_unique<ProjectsPanelController>(
      browser_, state_controller_,
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser->GetProfile()),
      threads_enabled
          ? contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
                browser->GetProfile())
          : nullptr);
  panel_controller_observer_.Observe(panel_controller_.get());

  controls_view_ = content_container_->AddChildView(
      std::make_unique<ProjectsPanelControlsView>(root_action_item_.get()));

  auto* tab_groups_container = content_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  tab_groups_container->SetOrientation(views::LayoutOrientation::kVertical);
  tab_groups_container_ = tab_groups_container;

  auto* groups_list_title =
      tab_groups_container_->AddChildView(std::make_unique<views::Label>());
  groups_list_title->SetText(l10n_util::GetStringUTF16(IDS_TAB_GROUPS_TITLE));
  groups_list_title->SetProperty(views::kElementIdentifierKey,
                                 kProjectsPanelTabGroupsListTitleElementId);
  SetListTitleProperties(*groups_list_title);
  SetListTitleContainerProperties(*groups_list_title);

  create_new_tab_group_button_ = tab_groups_container_->AddChildView(
      std::make_unique<ProjectsPanelNewTabGroupButton>(base::BindRepeating(
          &ProjectsPanelView::OnCreateNewTabGroupButtonPressed,
          base::Unretained(this))));
  create_new_tab_group_button_->SetProperty(
      views::kElementIdentifierKey, kProjectsPanelNewTabGroupButtonElementId);

  tab_groups_scroll_view_ =
      tab_groups_container_->AddChildView(std::make_unique<views::ScrollView>(
          views::ScrollView::ScrollWithLayers::kEnabled));
  tab_groups_view_ = tab_groups_scroll_view_->SetContents(
      std::make_unique<ProjectsPanelTabGroupsView>(
          root_action_item_.get(), action_view_controller_.get(),
          base::BindRepeating(&ProjectsPanelView::OnTabGroupButtonPressed,
                              base::Unretained(this)),
          base::BindRepeating(&ProjectsPanelView::OnTabGroupMoreButtonPressed,
                              base::Unretained(this)),
          base::BindRepeating(&ProjectsPanelView::OnTabGroupMoved,
                              base::Unretained(this)),
          base::BindRepeating(&ProjectsPanelView::OnTabGroupDragUpdated,
                              base::Unretained(this)),
          base::BindRepeating(&ProjectsPanelView::OnTabGroupDragExited,
                              base::Unretained(this))));
  tab_groups_view_->SetInsideBorderInsets(kListsInsideBorderInsets);
  SetScrollViewProperties(*tab_groups_scroll_view_);
  if (disable_animations_for_testing_) {
    tab_groups_view_->disable_animations_for_testing();  // IN-TEST
  }

  if (threads_enabled) {
    separator_ =
        content_container_->AddChildView(std::make_unique<views::Separator>());
    separator_->SetColorId(kColorProjectsPanelListsSeparator);

    auto* threads_container = content_container_->AddChildView(
        std::make_unique<views::FlexLayoutView>());
    threads_container->SetOrientation(views::LayoutOrientation::kVertical);
    threads_container_ = threads_container;

    auto* threads_title_container = threads_container_->AddChildView(
        std::make_unique<views::FlexLayoutView>());
    threads_title_container->SetOrientation(
        views::LayoutOrientation::kHorizontal);
    threads_title_container->SetCrossAxisAlignment(
        views::LayoutAlignment::kCenter);
    threads_title_container->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kPreferred));
    SetListTitleContainerProperties(*threads_title_container);

    auto* threads_list_title =
        threads_title_container->AddChildView(std::make_unique<views::Label>());
    threads_list_title->SetText(
        l10n_util::GetStringUTF16(IDS_RECENT_CHATS_TITLE));
    threads_list_title->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
    SetListTitleProperties(*threads_list_title);

    threads_activity_menu_button_ = threads_title_container->AddChildView(
        std::make_unique<views::MenuButton>(base::BindRepeating(
            &ProjectsPanelView::OnThreadsActivityMenuButtonPressed,
            base::Unretained(this))));
    threads_activity_menu_button_->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kBrowserToolsChromeRefreshIcon,
                                       kColorProjectsPanelButtonIcon,
                                       kThreadsActivityMenuButtonIconSize));
    threads_activity_menu_button_->SetPreferredSize(
        kThreadsActivityMenuButtonSize);
    threads_activity_menu_button_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_MORE_OPTIONS));
    threads_activity_menu_button_->SetProperty(
        views::kElementIdentifierKey,
        kProjectsPanelThreadsActivityButtonElementId);
    ConfigureInkDropForToolbar(threads_activity_menu_button_);

    views::ScrollView* threads_scroll_view =
        threads_container_->AddChildView(std::make_unique<views::ScrollView>(
            views::ScrollView::ScrollWithLayers::kEnabled));
    auto threads_view =
        std::make_unique<ProjectsPanelRecentThreadsView>(base::BindRepeating(
            &ProjectsPanelView::OnThreadButtonPressed, base::Unretained(this)));
    threads_view_ = threads_scroll_view->SetContents(std::move(threads_view));
    threads_view_->SetInsideBorderInsets(kListsInsideBorderInsets);
    SetScrollViewProperties(*threads_scroll_view);
    if (disable_animations_for_testing_) {
      threads_view_->disable_animations_for_testing();  // IN-TEST
    }

    threads_expand_button_ = threads_container_->AddChildView(
        std::make_unique<ProjectsPanelRecentThreadsExpandButton>(
            base::BindRepeating(&ProjectsPanelView::OnThreadExpandButtonPressed,
                                base::Unretained(this))));
    threads_expand_button_->SetProperty(views::kMarginsKey,
                                        kProjectsPanelRegionHorizontalMargins);

    threads_activity_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  }

  content_container_->SetLayoutManager(
      std::make_unique<ProjectsPanelViewLayout>(
          controls_view_, tab_groups_container_, threads_container_,
          separator_));

  resize_animation_.SetTweenType(gfx::Tween::Type::EASE_IN_OUT_EMPHASIZED);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  auto& accessibility = GetViewAccessibility();
  accessibility.SetRole(ax::mojom::Role::kPane);
  accessibility.SetName(l10n_util::GetStringUTF16(IDS_PROJECTS_PANEL));
  SetFocusBehavior(FocusBehavior::NEVER);

  SetVisible(false);
  SetPreferredSize(gfx::Size(projects_panel::kProjectsPanelMinWidth, 0));
  SetProperty(views::kElementIdentifierKey, kProjectsPanelViewElementId);
}

ProjectsPanelView::~ProjectsPanelView() {
  // Owned by subviews which are cleaned up after ProjectsPanelView is
  // cleaned up. Avoid dangling pointer.
  tab_groups_view_ = nullptr;
  threads_view_ = nullptr;
}

bool ProjectsPanelView::IsPositionInWindowCaption(const gfx::Point& point) {
  gfx::Point point_in_target = point;
  views::View::ConvertPointToTarget(this, controls_view_, &point_in_target);
  if (controls_view_->HitTestPoint(point_in_target)) {
    return controls_view_->IsPositionInWindowCaption(point_in_target);
  }

  return false;
}

void ProjectsPanelView::OnProjectsPanelStateChanged(
    ProjectsPanelStateController* state_controller) {
  TooltipTextChanged();
  controls_view_->UpdateTooltipText();

  const bool visible = state_controller->IsProjectsPanelVisible();

  if (visible) {
    views::Widget* widget = GetWidget();
    if (widget && widget->GetNativeWindow()) {
      event_monitor_ = views::EventMonitor::CreateWindowMonitor(
          &mouse_event_handler_, widget->GetNativeWindow(),
          {ui::EventType::kMousePressed, ui::EventType::kGestureTapDown});
    }

    if (!observing_focus_manager_ && GetFocusManager()) {
      GetFocusManager()->AddFocusChangeListener(this);
      last_focused_view_before_opening_.SetView(
          GetFocusManager()->GetFocusedView());
      observing_focus_manager_ = true;
    }

    // TODO(crbug.com/477602874): Have the panel view observe the controller and
    // pipe updates to the list.
    auto tab_groups = panel_controller_->GetTabGroups();
    tab_groups_view_->SetTabGroups(tab_groups);
    int num_threads_visible = 0;
    if (threads_view_) {
      const auto threads = panel_controller_->GetThreads();
      num_threads_visible =
          std::min(threads.size(), projects_panel::kMaxNumberOfRecentThreads);
      threads_view_->SetThreads(threads);

      // Hide the threads section when empty.
      const bool show_threads = show_threads_for_testing_ || threads.size() > 0;
      threads_container_->SetVisible(show_threads);
      separator_->SetVisible(show_threads);

      if (show_threads) {
        threads_expand_button_->SetExpanded(threads_view_->expanded());
        threads_expand_button_->SetVisible(
            threads.size() > projects_panel::kNumThreadsVisibleWhenCollapsed);
      }
    }

    base::UmaHistogramCounts100(
        "Projects.ProjectsPanel.TabGroups.CountOnPanelOpen", tab_groups.size());
    if (threads_view_) {
      base::UmaHistogramCustomCounts(
          "Projects.ProjectsPanel.Threads.CountOnPanelOpen",
          num_threads_visible, 1, projects_panel::kMaxNumberOfRecentThreads,
          50);
    }

    base::UmaHistogramBoolean("Projects.ProjectsPanel.OpenedToEmptyState",
                              tab_groups.empty() && num_threads_visible == 0);

    last_opened_time_ = base::TimeTicks::Now();
  } else {
    if (observing_focus_manager_ && GetFocusManager()) {
      GetFocusManager()->RemoveFocusChangeListener(this);
      if (last_focused_view_before_opening_) {
        GetFocusManager()->SetFocusedView(
            last_focused_view_before_opening_.view());
        last_focused_view_before_opening_.SetView(nullptr);
      }
      observing_focus_manager_ = false;
    }
    event_monitor_.reset();

    base::TimeDelta open_duration = base::TimeTicks::Now() - last_opened_time_;
    base::UmaHistogramCustomCounts("Projects.ProjectsPanel.TimeOpen",
                                   open_duration.InSeconds(), 1,
                                   base::Minutes(5).InSeconds(), 50);
  }

  if (disable_animations_for_testing_) {
    // Fast-forward the animation to its final state (1.0 = shown, 0.0 =
    // hidden).
    resize_animation_.SetSlideDuration(base::TimeDelta());
    resize_animation_.Reset(/*value=*/visible ? 1.0 : 0.0);
    SetVisible(visible);
    if (!visible) {
      AnimationEnded(&resize_animation_);
    }
  } else {
    if (visible) {
      SetVisible(true);
      resize_animation_.SetSlideDuration(kPanelShowAnimationDuration);
      resize_animation_.Show();
    } else {
      resize_animation_.SetSlideDuration(kPanelHideAnimationDuration);
      resize_animation_.Hide();
    }
  }

  if (visible && GetFocusManager()) {
    GetFocusManager()->SetFocusedView(this);
  }
}

double ProjectsPanelView::GetResizeAnimationValue() const {
  return resize_animation_.GetCurrentValue();
}

void ProjectsPanelView::SetTargetWidth(int target_width) {
  if (target_width_ == target_width) {
    return;
  }
  target_width_ = target_width;

  InvalidateLayout();
}

void ProjectsPanelView::SetIsElevated(bool elevated) {
  if (elevated_ == elevated) {
    return;
  }
  elevated_ = elevated;

  const int elevation = elevated_ ? kShadowElevation : 0;
  content_shadow_->shadow()->SetElevation(elevation);

  const int corner_radius = elevated_ ? kProjectPanelRightCornerRadius : 0;
  gfx::RoundedCornersF radii;
  if (base::i18n::IsRTL()) {
    radii = gfx::RoundedCornersF(corner_radius, 0, 0, corner_radius);
  } else {
    radii = gfx::RoundedCornersF(0, corner_radius, corner_radius, 0);
  }

  content_container_->layer()->SetRoundedCornerRadius(radii);
  content_container_->SetBackground(views::CreateRoundedRectBackground(
      projects_panel::kProjectsPanelBackgroundColor, radii));

  InvalidateLayout();
}

void ProjectsPanelView::Layout(PassKey) {
  const int visible_width = width();
  content_container_->SetBounds(-(target_width_ - visible_width), 0,
                                target_width_, height());

  // The content_container_ slides in from the edge of the window. In LTR this
  // is the left edge, and it should be clipped to that edge. However, we still
  // want the shadow to be visible on the opposite side, so we set a clip rect
  // that extends slightly beyond that edge.
  gfx::Rect clip_rect(0, 0, target_width_, height());
  if (base::i18n::IsRTL()) {
    clip_rect.Inset(gfx::Insets::TLBR(0, -kClipRectMarginForShadow, 0, 0));
  } else {
    clip_rect.Inset(gfx::Insets::TLBR(0, 0, 0, -kClipRectMarginForShadow));
  }
  layer()->SetClipRect(clip_rect);
}

void ProjectsPanelView::RemovedFromWidget() {
  if (observing_focus_manager_ && GetFocusManager()) {
    GetFocusManager()->RemoveFocusChangeListener(this);
    observing_focus_manager_ = false;
  }
}

bool ProjectsPanelView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    ClosePanel();
    return true;
  }
  return false;
}

views::FocusTraversable* ProjectsPanelView::GetPaneFocusTraversable() {
  return this;
}

views::FocusSearch* ProjectsPanelView::GetFocusSearch() {
  return focus_search_.get();
}

views::FocusTraversable* ProjectsPanelView::GetFocusTraversableParent() {
  return parent() ? parent()->GetFocusTraversable() : nullptr;
}

views::View* ProjectsPanelView::GetFocusTraversableParentView() {
  return this;
}

void ProjectsPanelView::AnimationProgressed(const gfx::Animation* animation) {
#if BUILDFLAG(IS_MAC)
  // On Mac, start fading in the close button when the panel has completed half
  // of its opening animation. Similarly when closing, fade out the button until
  // the panel has completed half of its closing animation.
  if (controls_view_) {
    const double value = animation->GetCurrentValue();
    controls_view_->SetButtonOpacity(std::max(0.0, (value - 0.5) * 2.0));
  }
#endif
  InvalidateLayout();
}

void ProjectsPanelView::AnimationEnded(const gfx::Animation* animation) {
  if (animation->GetCurrentValue() == 0.0) {
    SetVisible(false);
    if (on_close_animation_ended_callback_) {
      std::move(on_close_animation_ended_callback_).Run();
    }
  }
}

// We must also call AnimationEnded when an animation is canceled (which happens
// when the view is destroyed or a new animation is started mid-flight) to
// guarantee that the state is properly set to hidden.
void ProjectsPanelView::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void ProjectsPanelView::OnTabGroupsInitialized(
    const std::vector<tab_groups::SavedTabGroup>& tab_groups) {
  // TODO(crbug.com/477602874): Handle incremental data updates.
}

void ProjectsPanelView::OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                                        int index) {
  // TODO(crbug.com/477602874): Handle incremental data updates.
}

void ProjectsPanelView::OnTabGroupUpdated(
    const tab_groups::SavedTabGroup& group) {
  // TODO(crbug.com/477602874): Handle incremental data updates.
}

void ProjectsPanelView::OnTabGroupRemoved(const base::Uuid& sync_id,
                                          int old_index) {
  // TODO(crbug.com/477602874): Handle incremental data updates.
}

void ProjectsPanelView::OnTabGroupsReordered(
    const std::vector<tab_groups::SavedTabGroup>& tab_groups) {
  tab_groups_view_->SetTabGroups(tab_groups);
}

void ProjectsPanelView::OnThreadsInitialized(
    const std::vector<contextual_tasks::Thread>& threads) {
  if (threads_view_) {
    threads_view_->SetThreads(threads);
  }
}

// static
void ProjectsPanelView::set_threads_visible_for_testing(bool visible) {
  show_threads_for_testing_ = visible;
}

// static
void ProjectsPanelView::disable_animations_for_testing() {
  disable_animations_for_testing_ = true;
}

void ProjectsPanelView::ClosePanel(bool caused_by_focus_lost) {
  // If the panel is closing due to focus being lost (e.g., a tab group was
  // focused or a tab was activated), the last focused view before the panel was
  // opened should not be refocused.
  if (caused_by_focus_lost) {
    last_focused_view_before_opening_.SetView(nullptr);
  }

  // Ignore if the panel is already animating closed.
  if (!GetVisible() || resize_animation_.IsClosing()) {
    return;
  }

  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      kActionToggleProjectsPanel, root_action_item_);
  if (action_item) {
    action_item->InvokeAction();
  }
}

void ProjectsPanelView::OnTabGroupButtonPressed(const base::Uuid& group_guid) {
  base::RecordAction(
      base::UserMetricsAction("ProjectsPanel.TabGroups.OpenGroup"));
  panel_controller_->OpenTabGroup(group_guid);
  ClosePanel();
}

void ProjectsPanelView::OnTabGroupMoreButtonPressed(
    const base::Uuid& group_guid,
    views::MenuButton& button) {
  auto* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser_->GetProfile());

  auto saved_group = tab_group_service->GetGroup(group_guid);
  if (!saved_group.has_value()) {
    return;
  }

  tab_group_menu_model_ = std::make_unique<STGTabsMenuModelWithCallback>(
      browser_,
      tab_groups::TabGroupMenuContext::SAVED_TAB_GROUP_BUTTON_CONTEXT_MENU,
      base::BindRepeating(&ProjectsPanelView::ClosePanel,
                          base::Unretained(this),
                          /*closed_due_to_focus_lost=*/true));
  tab_group_menu_model_->Build(saved_group.value(), base::BindRepeating([]() {
                                 static int latest_command_id = 0;
                                 return latest_command_id++;
                               }));

  tab_group_menu_runner_ = std::make_unique<views::MenuRunner>(
      tab_group_menu_model_.get(),
      views::MenuRunner::CONTEXT_MENU | views::MenuRunner::IS_NESTED);
  tab_group_menu_runner_->RunMenuAt(
      button.GetWidget(), button.button_controller(),
      button.GetAnchorBoundsInScreen(), views::MenuAnchorPosition::kTopRight,
      ui::mojom::MenuSourceType::kNone);
}

void ProjectsPanelView::OnTabGroupMoved(const base::Uuid& group_guid,
                                        int new_index) {
  panel_controller_->MoveTabGroup(group_guid, new_index);
}

void ProjectsPanelView::OnCreateNewTabGroupButtonPressed() {
  base::RecordAction(base::UserMetricsAction(
      tab_groups_view_->num_tab_groups() > 0
          ? "ProjectsPanel.TabGroups.CreateNewGroup.WithExistingGroups"
          : "ProjectsPanel.TabGroups.CreateNewGroup.WithoutExistingGroups"));
  on_close_animation_ended_callback_ = base::BindOnce(
      // We must wait for the panel to fully close before executing the command
      // so we don't interfere with the panel's animation and UI state.
      [](base::WeakPtr<ProjectsPanelView> panel) {
        if (!panel) {
          return;
        }
        panel->browser_->GetFeatures()
            .browser_command_controller()
            ->ExecuteCommand(IDC_CREATE_NEW_TAB_GROUP);
      },
      weak_ptr_factory_.GetWeakPtr());
  ClosePanel();
}

void ProjectsPanelView::OnThreadButtonPressed(
    const std::string& thread_server_id,
    contextual_tasks::ThreadType thread_type) {
  switch (thread_type) {
    case contextual_tasks::ThreadType::kAiMode:
      base::RecordAction(
          base::UserMetricsAction("ProjectsPanel.Threads.OpenThread.AiMode"));
      break;
    case contextual_tasks::ThreadType::kGemini:
      base::RecordAction(
          base::UserMetricsAction("ProjectsPanel.Threads.OpenThread.Gemini"));
      break;
    case contextual_tasks::ThreadType::kUnknown:
      base::RecordAction(
          base::UserMetricsAction("ProjectsPanel.Threads.OpenThread.Unknown"));
      break;
  }
  panel_controller_->OpenThread(thread_server_id);
  ClosePanel();
}

void ProjectsPanelView::OnThreadExpandButtonPressed() {
  const bool expanded = !threads_view_->expanded();
  threads_view_->SetExpanded(expanded);
  threads_expand_button_->SetExpanded(expanded);
}

void ProjectsPanelView::OnThreadsActivityMenuButtonPressed() {
  threads_activity_menu_model_->Clear();

  if (state_controller_->CanShowGeminiThreads()) {
    threads_activity_menu_model_->AddItemWithIcon(
        kGeminiActivity,
        l10n_util::GetStringUTF16(IDS_PROJECTS_PANEL_GEMINI_ACTIVITY),
        ui::ImageModel::FromVectorIcon(
            projects_panel::GetIconForThreadType(
                contextual_tasks::ThreadType::kGemini),
            ui::kColorIcon, kThreadsActivityMenuIconSize));
    threads_activity_menu_model_->SetElementIdentifierAt(
        threads_activity_menu_model_->GetItemCount() - 1,
        kProjectsPanelThreadsActivityGeminiItemElementId);
  }

  if (state_controller_->CanShowAimThreads()) {
    threads_activity_menu_model_->AddItemWithIcon(
        kAiModeActivity,
        l10n_util::GetStringUTF16(IDS_PROJECTS_PANEL_AI_MODE_ACTIVITY),
        ui::ImageModel::FromVectorIcon(
            projects_panel::GetIconForThreadType(
                contextual_tasks::ThreadType::kAiMode),
            ui::kColorIcon, kThreadsActivityMenuIconSize));
    threads_activity_menu_model_->SetElementIdentifierAt(
        threads_activity_menu_model_->GetItemCount() - 1,
        kProjectsPanelThreadsActivityAiModeItemElementId);
  }

  threads_activity_menu_runner_ = std::make_unique<views::MenuRunner>(
      threads_activity_menu_model_.get(),
      views::MenuRunner::CONTEXT_MENU | views::MenuRunner::IS_NESTED);
  threads_activity_menu_runner_->RunMenuAt(
      GetWidget(), threads_activity_menu_button_->button_controller(),
      threads_activity_menu_button_->GetAnchorBoundsInScreen(),
      views::MenuAnchorPosition::kTopLeft, ui::mojom::MenuSourceType::kNone);
}

void ProjectsPanelView::OnTabGroupDragUpdated(const gfx::Point& location) {
  if (tab_groups_scroll_view_) {
    gfx::Point location_in_scroll_view = location;
    views::View::ConvertPointToTarget(tab_groups_view_, tab_groups_scroll_view_,
                                      &location_in_scroll_view);
    tab_groups_drag_scroll_handler_.OnDraggedTabGroupPositionUpdated(
        *tab_groups_scroll_view_, location_in_scroll_view);
  }
}

void ProjectsPanelView::OnTabGroupDragExited() {
  tab_groups_drag_scroll_handler_.StopScrolling();
}

void ProjectsPanelView::ExecuteCommand(int command_id, int event_flags) {
  GURL activity_url;
  switch (command_id) {
    case ThreadsActivityMenuCommandId::kGeminiActivity:
      activity_url = GURL(chrome::kMyActivityGeminiAppsUrl);
      break;
    case ThreadsActivityMenuCommandId::kAiModeActivity:
      activity_url = GURL(chrome::kMyActivityAiModeUrl);
      break;
    default:
      return;
  }
  browser_->OpenGURL(activity_url, WindowOpenDisposition::SINGLETON_TAB);
  ClosePanel();
}

bool ProjectsPanelView::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ProjectsPanelView::IsCommandIdEnabled(int command_id) const {
  return true;
}

ProjectsPanelView::MouseEventHandler::MouseEventHandler(
    ProjectsPanelView* owning_view)
    : owning_view_(owning_view) {}

ProjectsPanelView::MouseEventHandler::~MouseEventHandler() = default;

void ProjectsPanelView::MouseEventHandler::OnEvent(const ui::Event& event) {
  // Ignore mouse events when the panel is closed.
  if (!owning_view_->GetVisible()) {
    return;
  }

  if (event.type() == ui::EventType::kMousePressed ||
      event.type() == ui::EventType::kGestureTapDown) {
    if (!owning_view_->GetWidget()) {
      return;
    }

    auto point_in_view = event.AsLocatedEvent()->location();

    // Convert the point from the event's target to the panel's coordinates.
    views::View::ConvertPointFromWidget(owning_view_, &point_in_view);

    if (!owning_view_->GetLocalBounds().Contains(point_in_view)) {
      owning_view_->ClosePanel();
    }
  }
}

void ProjectsPanelView::OnWillChangeFocus(views::View* focused_before,
                                          views::View* focused_now) {}

void ProjectsPanelView::OnDidChangeFocus(views::View* focused_before,
                                         views::View* focused_now) {
  if (!GetVisible() || Contains(focused_now)) {
    return;
  }

  ClosePanel();
}

BEGIN_METADATA(ProjectsPanelView)
END_METADATA
