// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/full_restore/app_restore_data.h"

#include "components/full_restore/app_launch_info.h"

namespace full_restore {

namespace {

const char kEventFlagKey[] = "event_flag";
const char kContainerKey[] = "container";
const char kDispositionKey[] = "disposition";
const char kDisplayIdKey[] = "display_id";
const char kUrlKey[] = "url";
const char kIntentKey[] = "intent";
const char kActionKey[] = "action";
const char kMimeTypeKey[] = "mime_type";
const char kFileUrlsKey[] = "file_urls";
const char kActivityNameKey[] = "activity_name";
const char kDriveShareUrlKey[] = "drive_share_url";
const char kShareTextKey[] = "share_text";
const char kShareTitleKey[] = "share_title";
const char kFilePathsKey[] = "file_paths";
const char kActivationIndexKey[] = "index";
const char kDeskIdKey[] = "desk_id";
const char kRestoredBoundsKey[] = "restored_bounds";
const char kcurrentBoundsKey[] = "current_bounds";
const char kWindowStateTypeKey[] = "window_state_type";

// Converts |intent| to base::Value, e.g.:
// {
//    "action": "xx",
//    "url": "abc.com",
//    "mime_type": "text/plain",
//    "file_urls": "/abc, /a",
//    "activity_name": "yy",
//    "drive_share_url": "aa.com",
//    "share_text": "text",
//    "share_title": "title",
// }
base::Value ConvertIntentToValue(const apps::mojom::IntentPtr& intent) {
  DCHECK(intent);

  base::Value intent_value(base::Value::Type::DICTIONARY);
  if (intent->action.has_value() && !intent->action.value().empty())
    intent_value.SetStringKey(kActionKey, intent->action.value());

  if (intent->url.has_value()) {
    DCHECK(intent->url.value().is_valid());
    intent_value.SetStringKey(kUrlKey, intent->url.value().spec());
  }

  if (intent->mime_type.has_value() && !intent->mime_type.value().empty())
    intent_value.SetStringKey(kMimeTypeKey, intent->mime_type.value());

  if (intent->file_urls.has_value() && !intent->file_urls.value().empty()) {
    base::Value file_urls_list(base::Value::Type::LIST);
    for (auto& url : intent->file_urls.value()) {
      DCHECK(intent->drive_share_url.value().is_valid());
      file_urls_list.Append(base::Value(url.spec()));
    }
    intent_value.SetKey(kFileUrlsKey, std::move(file_urls_list));
  }

  if (intent->activity_name.has_value() &&
      !intent->activity_name.value().empty()) {
    intent_value.SetStringKey(kActivityNameKey, intent->activity_name.value());
  }

  if (intent->drive_share_url.has_value()) {
    DCHECK(intent->drive_share_url.value().is_valid());
    intent_value.SetStringKey(kDriveShareUrlKey,
                              intent->drive_share_url.value().spec());
  }

  if (intent->share_text.has_value() && !intent->share_text.value().empty())
    intent_value.SetStringKey(kShareTextKey, intent->share_text.value());

  if (intent->share_title.has_value() && !intent->share_title.value().empty())
    intent_value.SetStringKey(kShareTitleKey, intent->share_title.value());

  return intent_value;
}

// Converts |rect| to base::Value, e.g. { 0, 100, 200, 300 }.
base::Value ConvertRectToValue(const gfx::Rect& rect) {
  base::Value rect_list(base::Value::Type::LIST);
  rect_list.Append(base::Value(rect.x()));
  rect_list.Append(base::Value(rect.y()));
  rect_list.Append(base::Value(rect.width()));
  rect_list.Append(base::Value(rect.height()));
  return rect_list;
}

}  // namespace

AppRestoreData::AppRestoreData() = default;
AppRestoreData::~AppRestoreData() = default;

base::Value AppRestoreData::ConvertToValue() const {
  base::Value launch_info_dict(base::Value::Type::DICTIONARY);

  if (event_flag.has_value())
    launch_info_dict.SetIntKey(kEventFlagKey, event_flag.value());

  if (container.has_value())
    launch_info_dict.SetIntKey(kContainerKey, container.value());

  if (disposition.has_value())
    launch_info_dict.SetIntKey(kDispositionKey, disposition.value());

  if (display_id.has_value()) {
    launch_info_dict.SetStringKey(kDisplayIdKey,
                                  base::NumberToString(display_id.value()));
  }

  if (url.has_value())
    launch_info_dict.SetStringKey(kUrlKey, url.value().spec());

  if (intent.has_value() && intent.value())
    launch_info_dict.SetKey(kIntentKey, ConvertIntentToValue(intent.value()));

  if (file_paths.has_value() && !file_paths.value().empty()) {
    base::Value file_paths_list(base::Value::Type::LIST);
    for (auto& file_path : file_paths.value())
      file_paths_list.Append(base::Value(file_path.value()));
    launch_info_dict.SetKey(kFilePathsKey, std::move(file_paths_list));
  }

  if (activation_index.has_value())
    launch_info_dict.SetIntKey(kActivationIndexKey, activation_index.value());

  if (desk_id.has_value())
    launch_info_dict.SetIntKey(kDeskIdKey, desk_id.value());

  if (restored_bounds.has_value()) {
    launch_info_dict.SetKey(kRestoredBoundsKey,
                            ConvertRectToValue(restored_bounds.value()));
  }

  if (current_bounds.has_value()) {
    launch_info_dict.SetKey(kcurrentBoundsKey,
                            ConvertRectToValue(current_bounds.value()));
  }

  if (Window_state_type.has_value())
    launch_info_dict.SetIntKey(kWindowStateTypeKey, Window_state_type.value());

  return launch_info_dict;
}

AppRestoreData::AppRestoreData(std::unique_ptr<AppLaunchInfo> app_launch_info) {
  if (!app_launch_info)
    return;

  if (app_launch_info->event_flag.has_value())
    event_flag = app_launch_info->event_flag.value();

  if (app_launch_info->container.has_value())
    container = app_launch_info->container.value();

  if (app_launch_info->disposition.has_value())
    disposition = app_launch_info->disposition.value();

  if (app_launch_info->display_id.has_value())
    display_id = app_launch_info->display_id.value();

  if (app_launch_info->url.has_value())
    url = std::move(app_launch_info->url.value());

  if (app_launch_info->file_paths.has_value())
    file_paths = std::move(app_launch_info->file_paths.value());

  if (app_launch_info->intent.has_value())
    intent = std::move(app_launch_info->intent.value());
}

}  // namespace full_restore
