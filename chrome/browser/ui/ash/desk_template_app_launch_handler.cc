// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desk_template_app_launch_handler.h"

#include <string>

#include "base/notreached.h"
#include "components/full_restore/restore_data.h"

DeskTemplateAppLaunchHandler::DeskTemplateAppLaunchHandler(Profile* profile)
    : chromeos::AppLaunchHandler(profile) {}

DeskTemplateAppLaunchHandler::~DeskTemplateAppLaunchHandler() = default;

void DeskTemplateAppLaunchHandler::SetRestoreDataAndLaunch(
    std::unique_ptr<full_restore::RestoreData> restore_data) {
  // TODO(sammiequon) : Investigate if we can early return if a launch is
  // currently underway.
  restore_data_ = std::move(restore_data);
  if (HasRestoreData())
    LaunchApps();
}

base::WeakPtr<chromeos::AppLaunchHandler>
DeskTemplateAppLaunchHandler::GetWeakPtrAppLaunchHandler() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DeskTemplateAppLaunchHandler::LaunchBrowser() {
  // TODO: Launch browser with `restore_id`.
  NOTIMPLEMENTED();
}

void DeskTemplateAppLaunchHandler::LaunchArcApp(
    const std::string& app_id,
    const ::full_restore::RestoreData::LaunchList& launch_list) {
  // ARC is not currently supported for desk templates.
  NOTREACHED();
}

void DeskTemplateAppLaunchHandler::RecordRestoredAppLaunch(
    apps::AppTypeName app_type_name) {
  // TODO: Add UMA Histogram.
  NOTIMPLEMENTED();
}

void DeskTemplateAppLaunchHandler::RecordArcGhostWindowLaunch(
    bool is_arc_ghost_window) {
  // ARC is not currently supported for desk templates.
  NOTREACHED();
}
