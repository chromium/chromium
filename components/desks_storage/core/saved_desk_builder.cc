// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/saved_desk_builder.h"

#include "ash/public/cpp/desk_template.h"
#include "base/guid.h"
#include "base/time/time.h"
#include "components/app_restore/app_launch_info.h"
#include "components/desks_storage/core/saved_desk_test_util.h"

namespace desks_storage {

SavedDeskBuilder::SavedDeskBuilder()
    : desk_name_("unnamed desk"),
      desk_source_(ash::DeskTemplateSource::kUser),
      desk_type_(ash::DeskTemplateType::kTemplate),
      restore_data_(std::make_unique<app_restore::RestoreData>()) {
  desk_uuid_ = base::GUID::GenerateRandomV4().AsLowercaseString();
  created_time_ = base::Time::Now();
};

SavedDeskBuilder::~SavedDeskBuilder(){};

std::unique_ptr<ash::DeskTemplate> SavedDeskBuilder::Build() {
  auto desk_template = std::make_unique<ash::DeskTemplate>(
      desk_uuid_, desk_source_, desk_name_, created_time_, desk_type_);

  desk_template->set_desk_restore_data(std::move(restore_data_));

  return desk_template;
}

SavedDeskBuilder& SavedDeskBuilder::SetUuid(const std::string& uuid) {
  desk_uuid_ = uuid;
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

SavedDeskBuilder& SavedDeskBuilder::SetCreatedTime(base::Time& created_time) {
  created_time_ = created_time;
  return *this;
}

SavedDeskBuilder& SavedDeskBuilder::AddAshBrowserAppWindow(
    int window_id,
    std::vector<GURL> urls) {
  saved_desk_test_util::AddBrowserWindow(/*is_lacros=*/false, window_id, urls,
                                         restore_data_.get());
  return *this;
}

SavedDeskBuilder& SavedDeskBuilder::AddLacrosBrowserAppWindow(
    int window_id,
    std::vector<GURL> urls) {
  saved_desk_test_util::AddBrowserWindow(/*is_lacros=*/true, window_id, urls,
                                         restore_data_.get());
  return *this;
}

SavedDeskBuilder& SavedDeskBuilder::AddAshPwaAppWindow(int window_id,
                                                       const std::string url) {
  saved_desk_test_util::AddPwaWindow(/*is_lacros=*/false, window_id, url,
                                     restore_data_.get());
  return *this;
}

SavedDeskBuilder& SavedDeskBuilder::AddLacrosPwaAppWindow(
    int window_id,
    const std::string url) {
  saved_desk_test_util::AddPwaWindow(/*is_lacros=*/true, window_id, url,
                                     restore_data_.get());
  return *this;
}

SavedDeskBuilder& SavedDeskBuilder::AddChromeAppWindow(
    int window_id,
    const std::string app_id) {
  saved_desk_test_util::AddGenericAppWindow(window_id, app_id,
                                            restore_data_.get());
  return *this;
}

SavedDeskBuilder& SavedDeskBuilder::AddGenericAppWindow(
    int window_id,
    const std::string app_id) {
  saved_desk_test_util::AddGenericAppWindow(window_id, app_id,
                                            restore_data_.get());
  return *this;
}

}  // namespace desks_storage
