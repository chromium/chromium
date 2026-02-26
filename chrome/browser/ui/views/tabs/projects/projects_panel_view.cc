// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_menu_utils.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_tabs_menu_model.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_controls_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_recent_threads_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_view.h"
#include "chrome/browser/ui/views/tabs/vertical/top_container_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/saved_tab_groups/public/features.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_shadow.h"

namespace {
constexpr int kClipRectRightMarginForShadow = 32;
constexpr int kProjectPanelRightCornerRadius = 16;
constexpr gfx::Insets kRegionInteriorMargins = gfx::Insets::VH(12, 12);
constexpr int kShadowElevation = 2;
// The padding around a list header.
constexpr gfx::Insets kListHeaderPadding = gfx::Insets::VH(10, 20);

constexpr base::TimeDelta kPanelShowAnimationDuration = base::Milliseconds(250);
constexpr base::TimeDelta kPanelHideAnimationDuration = base::Milliseconds(200);

static bool disable_animations_for_testing_ = false;

// Assigns shared list title properties.
void SetListTitleProperties(views::Label& label) {
  label.SetTextStyle(views::style::TextStyle::STYLE_HEADLINE_5);
  label.SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  label.SetProperty(views::kMarginsKey, kListHeaderPadding);
}

void SetScrollViewProperties(views::ScrollView& scroll_view) {
  scroll_view.SetBackgroundColor(projects_panel::kProjectsPanelBackgroundColor);
  scroll_view.SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view.SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll_view.SetOverflowGradientMask(
      views::ScrollView::GradientDirection::kVertical);
  scroll_view.SetUseContentsPreferredSize(true);
  scroll_view.SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred));
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

ProjectsPanelView::ProjectsPanelView(BrowserWindowInterface* browser,
                                     actions::ActionItem* root_action_item)
    : browser_(browser),
      root_action_item_(root_action_item),
      action_view_controller_(std::make_unique<views::ActionViewController>()),
      resize_animation_(this) {
  // The vertical tab strip contains ScrollViews that paint to a layer. This
  // view must also paint to a layer to ensure it overlays those components.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  content_container_ = AddChildView(std::make_unique<views::View>());
  content_container_->SetPaintToLayer();
  content_container_->layer()->SetFillsBoundsOpaquely(false);
  content_container_->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(kRegionInteriorMargins)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred));
  content_shadow_ =
      std::make_unique<views::ViewShadow>(content_container_, kShadowElevation);
  content_shadow_->SetRoundedCornerRadius(kProjectPanelRightCornerRadius);

  // Apply the elevated state by default.
  SetIsElevated(true);

  panel_controller_ = std::make_unique<ProjectsPanelController>(
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser->GetProfile()),
      tab_groups::IsThreadsInProjectsPanelEnabled()
          ? contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
                browser->GetProfile())
          : nullptr);
  panel_controller_observer_.Observe(panel_controller_.get());

  controls_view_ = content_container_->AddChildView(
      std::make_unique<ProjectsPanelControlsView>(
          root_action_item_.get(), action_view_controller_.get()));

  auto* groups_list_title =
      content_container_->AddChildView(std::make_unique<views::Label>());
  groups_list_title->SetText(l10n_util::GetStringUTF16(IDS_TAB_GROUPS_TITLE));
  groups_list_title->SetProperty(views::kElementIdentifierKey,
                                 kProjectsPanelTabGroupsListTitleElementId);
  SetListTitleProperties(*groups_list_title);

  views::ScrollView* tab_groups_scroll_view =
      content_container_->AddChildView(std::make_unique<views::ScrollView>(
          views::ScrollView::ScrollWithLayers::kEnabled));

  tab_groups_view_ = tab_groups_scroll_view->SetContents(
      std::make_unique<ProjectsPanelTabGroupsView>(
          root_action_item_.get(), action_view_controller_.get(),
          base::BindRepeating(&ProjectsPanelView::OnTabGroupButtonPressed,
                              base::Unretained(this)),
          base::BindRepeating(&ProjectsPanelView::OnTabGroupMoreButtonPressed,
                              base::Unretained(this)),
          base::BindRepeating(&ProjectsPanelView::OnTabGroupMoved,
                              base::Unretained(this)),
          base::BindRepeating(
              &ProjectsPanelView::OnCreateNewTabGroupButtonPressed,
              base::Unretained(this))));
  SetScrollViewProperties(*tab_groups_scroll_view);
  if (disable_animations_for_testing_) {
    tab_groups_view_->disable_animations_for_testing();  // IN-TEST
  }

  if (tab_groups::IsThreadsInProjectsPanelEnabled()) {
    auto* threads_list_title =
        content_container_->AddChildView(std::make_unique<views::Label>());
    threads_list_title->SetText(
        l10n_util::GetStringUTF16(IDS_RECENT_CHATS_TITLE));
    SetListTitleProperties(*threads_list_title);

    views::ScrollView* threads_scroll_view =
        content_container_->AddChildView(std::make_unique<views::ScrollView>(
            views::ScrollView::ScrollWithLayers::kEnabled));
    auto threads_view = std::make_unique<ProjectsPanelRecentThreadsView>(
        panel_controller_->GetThreads());
    threads_view_ = threads_scroll_view->SetContents(std::move(threads_view));
    SetScrollViewProperties(*threads_scroll_view);
  }

  resize_animation_.SetTweenType(gfx::Tween::Type::EASE_IN_OUT_EMPHASIZED);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

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

  const bool visible = state_controller->IsProjectsPanelVisible();

  if (visible) {
    views::Widget* widget = GetWidget();
    if (widget && widget->GetNativeWindow()) {
      event_monitor_ = views::EventMonitor::CreateWindowMonitor(
          &mouse_event_handler_, widget->GetNativeWindow(),
          {ui::EventType::kMousePressed, ui::EventType::kGestureTapDown});
    }

    // TODO(crbug.com/477602874): Have the panel view observe the controller and
    // pipe updates to the list.
    tab_groups_view_->SetTabGroups(panel_controller_->GetTabGroups());
    if (threads_view_) {
      threads_view_->SetThreads(panel_controller_->GetThreads());
    }
  } else {
    event_monitor_.reset();
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
    return;
  }

  if (visible) {
    SetVisible(true);
    resize_animation_.SetSlideDuration(kPanelShowAnimationDuration);
    resize_animation_.Show();
  } else {
    resize_animation_.SetSlideDuration(kPanelHideAnimationDuration);
    resize_animation_.Hide();
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
  content_container_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(0, corner_radius, corner_radius, 0));
  content_container_->SetBackground(views::CreateRoundedRectBackground(
      projects_panel::kProjectsPanelBackgroundColor,
      gfx::RoundedCornersF(0, corner_radius, corner_radius, 0)));

  InvalidateLayout();
}

void ProjectsPanelView::Layout(PassKey) {
  const int visible_width = width();
  content_container_->SetBounds(-(target_width_ - visible_width), 0,
                                target_width_, height());

  // The content_container_ slides in from the left and should be clipped to the
  // left edge of the panel. However, we still want the shadow to be visible on
  // the right, so we set a clip rect that starts at x=0 but extends slightly
  // beyond the right edge.
  layer()->SetClipRect(
      gfx::Rect(0, 0, target_width_ + kClipRectRightMarginForShadow, height()));
}

bool ProjectsPanelView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    ClosePanel();
    return true;
  }
  return false;
}

void ProjectsPanelView::AnimationProgressed(const gfx::Animation* animation) {
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
void ProjectsPanelView::disable_animations_for_testing() {
  disable_animations_for_testing_ = true;
}

void ProjectsPanelView::ClosePanel() {
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
  panel_controller_->OpenTabGroup(group_guid, browser_);
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
                          base::Unretained(this)));
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
      ui::mojom::MenuSourceType::kMouse);
}

void ProjectsPanelView::OnTabGroupMoved(const base::Uuid& group_guid,
                                        int new_index) {
  panel_controller_->MoveTabGroup(group_guid, new_index);
}

void ProjectsPanelView::OnCreateNewTabGroupButtonPressed() {
  on_close_animation_ended_callback_ = base::BindOnce(
      [](base::WeakPtr<ProjectsPanelView> panel) {
        if (!panel) {
          return;
        }
        panel->browser_->GetBrowserForMigrationOnly()
            ->command_controller()
            ->ExecuteCommand(IDC_CREATE_NEW_TAB_GROUP);
      },
      weak_ptr_factory_.GetWeakPtr());
  ClosePanel();
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

BEGIN_METADATA(ProjectsPanelView)
END_METADATA
