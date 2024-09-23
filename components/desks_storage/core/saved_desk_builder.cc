// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/saved_desk_builder.h"

#include "ash/public/cpp/desk_template.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace desks_storage {

namespace {

// Fills restore data by invoking `builder`s build method.  Drops data if the
// build fails.
void FillRestoreData(BuiltApp& app,
                     app_restore::RestoreData* out_restore_data) {
  if (app.status != BuiltApp::Status::kOk)
    return;

  // Something has gone very wrong if we built with OK status and don't have
  // a `window_id` or `app_id` which are needed to construct a valid launch
  // info.
  DCHECK(app.launch_info);
  DCHECK(app.window_info);

  int32_t window_id = app.launch_info->window_id.value();
  std::string app_id = app.launch_info->app_id;

  out_restore_data->AddAppLaunchInfo(std::move(app.launch_info));
  out_restore_data->ModifyWindowInfo(app_id, window_id, *app.window_info.get());
}

}  // namespace

// SavedDeskGenericAppBuilder implementation.
SavedDeskGenericAppBuilder::SavedDeskGenericAppBuilder() = default;
SavedDeskGenericAppBuilder::SavedDeskGenericAppBuilder(
    SavedDeskGenericAppBuilder&&) = default;
SavedDeskGenericAppBuilder& SavedDeskGenericAppBuilder::operator=(
    SavedDeskGenericAppBuilder&&) = default;
SavedDeskGenericAppBuilder::~SavedDeskGenericAppBuilder() = default;

SavedDeskGenericAppBuilder& SavedDeskGenericAppBuilder::SetWindowBound(
    gfx::Rect bounds) {
  window_bounds_ = bounds;
  return *this;
}

SavedDeskGenericAppBuilder& SavedDeskGenericAppBuilder::SetWindowState(
    chromeos::WindowStateType state) {
  window_show_state_ = state;
  return *this;
}

SavedDeskGenericAppBuilder&
SavedDeskGenericAppBuilder::SetPreMinimizedWindowState(
    ui::mojom::WindowShowState state) {
  pre_minimized_window_show_state_ = state;
  return *this;
}

SavedDeskGenericAppBuilder& SavedDeskGenericAppBuilder::SetAppId(
    std::string app_id) {
  app_id_ = app_id;
  return *this;
}

SavedDeskGenericAppBuilder& SavedDeskGenericAppBuilder::SetZIndex(int index) {
  z_index_ = index;
  return *this;
}

SavedDeskGenericAppBuilder& SavedDeskGenericAppBuilder::SetWindowId(
    int window_id) {
  window_id_ = window_id;
  return *this;
}

SavedDeskGenericAppBuilder& SavedDeskGenericAppBuilder::SetDisplayId(
    int64_t display_id) {
  display_id_ = display_id;
  return *this;
}

SavedDeskGenericAppBuilder& SavedDeskGenericAppBuilder::SetLaunchContainer(
    apps::LaunchContainer container) {
  launch_conatiner_ = container;
  return *this;
}

SavedDeskGenericAppBuilder&
SavedDeskGenericAppBuilder::SetWindowOpenDisposition(
    WindowOpenDisposition disposition) {
  disposition_ = disposition;
  return *this;
}

SavedDeskGenericAppBuilder& SavedDeskGenericAppBuilder::SetName(
    std::string name) {
  name_ = name;
  return *this;
}

SavedDeskGenericAppBuilder& SavedDeskGenericAppBuilder::SetSnapPercentage(
    int percentage) {
  snap_percentage_ = percentage;
  return *this;
}

SavedDeskGenericAppBuilder& SavedDeskGenericAppBuilder::SetEventFlag(
    int32_t event_flag) {
  event_flag_ = event_flag;
  return *this;
}

BuiltApp SavedDeskGenericAppBuilder::Build() {
  if (!window_id_)
    return BuiltApp(BuiltApp::Status::kNoWindowId, nullptr, nullptr);

  if (!app_id_)
    app_id_ = GetAppId();

  auto app_launch_info = std::make_unique<app_restore::AppLaunchInfo>(
      GetAppId(), window_id_.value());
  auto window_info = std::make_unique<app_restore::WindowInfo>();

  window_info->window_state_type = window_show_state_;
  window_info->pre_minimized_show_state_type = pre_minimized_window_show_state_;
  window_info->current_bounds = window_bounds_;
  window_info->activation_index = z_index_;
  window_info->snap_percentage = snap_percentage_;
  window_info->display_id = display_id_;

  if (launch_conatiner_) {
    app_launch_info->container =
        std::optional<int32_t>(static_cast<int32_t>(launch_conatiner_.value()));
  }

  if (disposition_) {
    app_launch_info->disposition =
        std::optional<int32_t>(static_cast<int32_t>(disposition_.value()));
  }

  if (event_flag_) {
    app_launch_info->event_flag = event_flag_.value();
  }

  app_launch_info->browser_extra_info.app_name = name_;
  app_launch_info->window_id = window_id_;

  return BuiltApp(BuiltApp::Status::kOk, std::move(window_info),
                  std::move(app_launch_info));
}

const std::string& SavedDeskGenericAppBuilder::GetAppId() {
  if (app_id_)
    return app_id_.value();

  app_id_ = base::Uuid::GenerateRandomV4().AsLowercaseString();
  return app_id_.value();
}

// SavedDeskGenericAppBuilder::BuiltApp implementation
BuiltApp::BuiltApp(BuiltApp::Status status,
                   std::unique_ptr<app_restore::WindowInfo> window_info,
                   std::unique_ptr<app_restore::AppLaunchInfo> launch_info)
    : status(status),
      window_info(std::move(window_info)),
      launch_info(std::move(launch_info)) {}
BuiltApp::BuiltApp(BuiltApp&&) = default;
BuiltApp& BuiltApp::operator=(BuiltApp&&) = default;
BuiltApp::~BuiltApp() = default;

// TabGroupWithStatus implementation
SavedDeskTabGroupBuilder::TabGroupWithStatus::TabGroupWithStatus(
    TabGroupBuildStatus status,
    std::unique_ptr<tab_groups::TabGroupInfo> tab_group)
    : status(status), tab_group(std::move(tab_group)) {}
SavedDeskTabGroupBuilder::TabGroupWithStatus::~TabGroupWithStatus() = default;

// SavedDeskTabGroup builder implementation.
SavedDeskTabGroupBuilder::SavedDeskTabGroupBuilder() = default;
SavedDeskTabGroupBuilder::SavedDeskTabGroupBuilder(SavedDeskTabGroupBuilder&&) =
    default;
SavedDeskTabGroupBuilder::~SavedDeskTabGroupBuilder() = default;

SavedDeskTabGroupBuilder& SavedDeskTabGroupBuilder::SetRange(gfx::Range range) {
  range_ = range;
  return *this;
}

SavedDeskTabGroupBuilder& SavedDeskTabGroupBuilder::SetTitle(
    std::string title) {
  title_ = title;
  return *this;
}

SavedDeskTabGroupBuilder& SavedDeskTabGroupBuilder::SetColor(
    tab_groups::TabGroupColorId color) {
  color_ = color;
  return *this;
}

SavedDeskTabGroupBuilder& SavedDeskTabGroupBuilder::SetIsCollapsed(
    bool is_collapsed) {
  is_collapsed_ = is_collapsed;
  return *this;
}

SavedDeskTabGroupBuilder::TabGroupWithStatus SavedDeskTabGroupBuilder::Build() {
  if (!range_ || !title_ || !color_ || !is_collapsed_)
    return TabGroupWithStatus(TabGroupBuildStatus::kNotAllFieldsSet, nullptr);

  auto tab_group = std::make_unique<tab_groups::TabGroupInfo>(
      range_.value(),
      tab_groups::TabGroupVisualData(base::UTF8ToUTF16(title_.value()),
                                     color_.value(), is_collapsed_.value()));

  return TabGroupWithStatus(TabGroupBuildStatus::kOk, std::move(tab_group));
}

// SavedDeskBrowserBuilder implementation.
SavedDeskBrowserBuilder::SavedDeskBrowserBuilder() = default;
SavedDeskBrowserBuilder::~SavedDeskBrowserBuilder() = default;

SavedDeskBrowserBuilder& SavedDeskBrowserBuilder::SetActiveTabIndex(int index) {
  active_tab_index_ = index;
  return *this;
}

SavedDeskBrowserBuilder& SavedDeskBrowserBuilder::SetFirstNonPinnedTabIndex(
    int index) {
  first_non_pinned_tab_index_ = index;
  return *this;
}

SavedDeskBrowserBuilder& SavedDeskBrowserBuilder::SetUrls(
    std::vector<GURL> urls) {
  urls_ = urls;
  return *this;
}

SavedDeskBrowserBuilder& SavedDeskBrowserBuilder::SetIsLacros(bool is_lacros) {
  is_lacros_ = is_lacros;
  return *this;
}

SavedDeskBrowserBuilder& SavedDeskBrowserBuilder::SetLacrosProfileId(
    uint64_t lacros_profile_id) {
  lacros_profile_id_ = lacros_profile_id;
  return *this;
}

SavedDeskBrowserBuilder& SavedDeskBrowserBuilder::SetIsApp(bool is_app) {
  is_app_ = is_app;
  return *this;
}

SavedDeskBrowserBuilder& SavedDeskBrowserBuilder::AddTabGroupBuilder(
    SavedDeskTabGroupBuilder tab_group) {
  tab_group_builders_.push_back(std::move(tab_group));
  return *this;
}

BuiltApp SavedDeskBrowserBuilder::Build() {
  generic_builder_.SetAppId(is_lacros_ ? app_constants::kLacrosAppId
                                       : app_constants::kChromeAppId);

  BuiltApp generic_app = generic_builder_.Build();
  if (generic_app.status != BuiltApp::Status::kOk)
    return BuiltApp(generic_app.status, nullptr, nullptr);

  generic_app.launch_info->browser_extra_info.urls = urls_;
  generic_app.launch_info->browser_extra_info.active_tab_index =
      active_tab_index_;
  generic_app.launch_info->browser_extra_info.first_non_pinned_tab_index =
      first_non_pinned_tab_index_;
  generic_app.launch_info->browser_extra_info.app_type_browser = is_app_;
  generic_app.launch_info->browser_extra_info.lacros_profile_id =
      lacros_profile_id_;
  for (auto& tab_group : tab_group_builders_) {
    SavedDeskTabGroupBuilder::TabGroupWithStatus built_group =
        tab_group.Build();
    if (built_group.status !=
        SavedDeskTabGroupBuilder::TabGroupBuildStatus::kOk) {
      continue;
    }
    DCHECK(built_group.tab_group);

    generic_app.launch_info->browser_extra_info.tab_group_infos.push_back(
        *built_group.tab_group.release());
  }

  return BuiltApp(BuiltApp::Status::kOk, std::move(generic_app.window_info),
                  std::move(generic_app.launch_info));
}

SavedDeskBrowserBuilder& SavedDeskBrowserBuilder::SetGenericBuilder(
    SavedDeskGenericAppBuilder& generic_builder) {
  generic_builder_ = std::move(generic_builder);
  return *this;
}

// SavedDeskArcAppBuilder implementation
SavedDeskArcAppBuilder::SavedDeskArcAppBuilder() = default;
SavedDeskArcAppBuilder::~SavedDeskArcAppBuilder() = default;

SavedDeskArcAppBuilder& SavedDeskArcAppBuilder::SetAppId(std::string app_id) {
  app_id_ = app_id;
  return *this;
}

SavedDeskArcAppBuilder& SavedDeskArcAppBuilder::SetMinimumSize(gfx::Size size) {
  minimum_size_ = size;
  return *this;
}

SavedDeskArcAppBuilder& SavedDeskArcAppBuilder::SetMaximumSize(gfx::Size size) {
  maximum_size_ = size;
  return *this;
}

SavedDeskArcAppBuilder& SavedDeskArcAppBuilder::SetBoundsInRoot(
    gfx::Rect bounds) {
  bounds_in_root_ = bounds;
  return *this;
}

BuiltApp SavedDeskArcAppBuilder::Build() {
  BuiltApp generic_app = generic_builder_.SetAppId(app_id_.value()).Build();

  if (generic_app.status != BuiltApp::Status::kOk)
    return BuiltApp(generic_app.status, nullptr, nullptr);

  generic_app.window_info->arc_extra_info = {.maximum_size = maximum_size_,
                                             .minimum_size = minimum_size_,
                                             .bounds_in_root = bounds_in_root_};
  return BuiltApp(BuiltApp::Status::kOk, std::move(generic_app.window_info),
                  std::move(generic_app.launch_info));
}

SavedDeskArcAppBuilder& SavedDeskArcAppBuilder::SetGenericBuilder(
    SavedDeskGenericAppBuilder& generic_builder) {
  generic_builder_ = std::move(generic_builder);
  return *this;
}

// SavedDeskBuilder implementation.
SavedDeskBuilder::SavedDeskBuilder()
    : desk_name_("unnamed desk"),
      desk_source_(ash::DeskTemplateSource::kUser),
      desk_type_(ash::DeskTemplateType::kTemplate) {
  desk_uuid_ = base::Uuid::GenerateRandomV4();
  created_time_ = base::Time::Now();
  updated_time_ = created_time_;
}
SavedDeskBuilder::~SavedDeskBuilder() = default;

std::unique_ptr<ash::DeskTemplate> SavedDeskBuilder::Build() {
  auto desk_template = std::make_unique<ash::DeskTemplate>(
      desk_uuid_, desk_source_, desk_name_, created_time_, desk_type_,
      policy_should_launch_on_startup_, policy_value_.Clone());

  if (has_updated_time_) {
    desk_template->set_updated_time(updated_time_);
  }
  if (lacros_profile_id_) {
    desk_template->set_lacros_profile_id(*lacros_profile_id_);
  }

  auto restore_data = std::make_unique<app_restore::RestoreData>();

  for (auto& app : built_apps_)
    FillRestoreData(app, restore_data.get());

  desk_template->set_desk_restore_data(std::move(restore_data));

  return desk_template;
}

SavedDeskBuilder& SavedDeskBuilder::SetUuid(const std::string& uuid) {
  desk_uuid_ = base::Uuid::ParseCaseInsensitive(uuid);
  return *this;
}

SavedDeskBuilder& SavedDeskBuilder::SetName(const std::string& name) {
  desk_name_ = name;
  return *this;
}

SavedDeskBuilder& SavedDeskBuilder::SetType(ash::DeskTemplateType desk_type) {
  desk_type_ = desk_type;
  return *this;
}

SavedDeskBuilder& SavedDeskBuilder::SetSource(
    ash::DeskTemplateSource desk_source) {
  desk_source_ = desk_source;
  return *this;
}

SavedDeskBuilder& SavedDeskBuilder::SetCreatedTime(
    const base::Time& created_time) {
  created_time_ = created_time;

  return *this;
}

SavedDeskBuilder& SavedDeskBuilder::SetUpdatedTime(
    const base::Time& updated_time) {
  has_updated_time_ = true;
  updated_time_ = updated_time;
  return *this;
}

SavedDeskBuilder& SavedDeskBuilder::SetPolicyValue(const base::Value& value) {
  policy_value_ = value.Clone();
  return *this;
}

SavedDeskBuilder& SavedDeskBuilder::SetPolicyShouldLaunchOnStartup(
    bool should_launch_on_startup) {
  policy_should_launch_on_startup_ = should_launch_on_startup;
  return *this;
}

SavedDeskBuilder& SavedDeskBuilder::SetLacrosProfileId(
    uint64_t lacros_profile_id) {
  lacros_profile_id_ = lacros_profile_id;
  return *this;
}

SavedDeskBuilder& SavedDeskBuilder::AddAppWindow(BuiltApp built_app) {
  built_apps_.push_back(std::move(built_app));
  return *this;
}

}  // namespace desks_storage
