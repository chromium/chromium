// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent.h"

#include "base/files/safe_base_name.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

IntentFile::IntentFile(const GURL& url) : url(url) {}

IntentFile::~IntentFile() = default;

Intent::Intent(const std::string& action) : action(action) {}

Intent::~Intent() = default;

IntentFilePtr ConvertMojomIntentFileToIntentFile(
    const apps::mojom::IntentFilePtr& mojom_intent_file) {
  if (!mojom_intent_file) {
    return nullptr;
  }

  auto intent_file = std::make_unique<IntentFile>(mojom_intent_file->url);
  if (mojom_intent_file->mime_type.has_value()) {
    intent_file->mime_type = mojom_intent_file->mime_type.value();
  }
  if (mojom_intent_file->file_name.has_value()) {
    intent_file->file_name = mojom_intent_file->file_name.value();
  }
  intent_file->file_size = mojom_intent_file->file_size;
  intent_file->is_directory = GetOptionalBool(mojom_intent_file->is_directory);
  return intent_file;
}

apps::mojom::IntentFilePtr ConvertIntentFileToMojomIntentFile(
    const IntentFilePtr& intent_file) {
  if (!intent_file) {
    return nullptr;
  }

  auto mojom_intent_file = apps::mojom::IntentFile::New();
  mojom_intent_file->url = intent_file->url;
  if (intent_file->mime_type.has_value()) {
    mojom_intent_file->mime_type = intent_file->mime_type.value();
  }
  if (intent_file->file_name.has_value()) {
    mojom_intent_file->file_name = intent_file->file_name.value();
  }
  mojom_intent_file->file_size = intent_file->file_size;
  mojom_intent_file->is_directory =
      GetMojomOptionalBool(intent_file->is_directory);
  return mojom_intent_file;
}

IntentPtr ConvertMojomIntentToIntent(
    const apps::mojom::IntentPtr& mojom_intent) {
  if (!mojom_intent) {
    return nullptr;
  }

  auto intent = std::make_unique<Intent>(mojom_intent->action);

  if (mojom_intent->url.has_value()) {
    intent->url = mojom_intent->url.value();
  }
  if (mojom_intent->mime_type.has_value()) {
    intent->mime_type = mojom_intent->mime_type.value();
  }
  if (mojom_intent->files.has_value()) {
    for (const auto& file : mojom_intent->files.value()) {
      intent->files.push_back(ConvertMojomIntentFileToIntentFile(file));
    }
  }
  if (mojom_intent->activity_name.has_value()) {
    intent->activity_name = mojom_intent->activity_name.value();
  }
  if (mojom_intent->drive_share_url.has_value()) {
    intent->drive_share_url = mojom_intent->drive_share_url.value();
  }
  if (mojom_intent->share_text.has_value()) {
    intent->share_text = mojom_intent->share_text.value();
  }
  if (mojom_intent->share_title.has_value()) {
    intent->share_title = mojom_intent->share_title.value();
  }
  if (mojom_intent->start_type.has_value()) {
    intent->start_type = mojom_intent->start_type.value();
  }
  if (mojom_intent->categories.has_value()) {
    for (const auto& category : mojom_intent->categories.value()) {
      intent->categories.push_back(category);
    }
  }
  if (mojom_intent->data.has_value()) {
    intent->data = mojom_intent->data.value();
  }
  intent->ui_bypassed = GetOptionalBool(mojom_intent->ui_bypassed);
  if (mojom_intent->extras.has_value()) {
    for (const auto& extra : mojom_intent->extras.value()) {
      intent->extras[extra.first] = extra.second;
    }
  }
  return intent;
}

apps::mojom::IntentPtr ConvertIntentToMojomIntent(const IntentPtr& intent) {
  if (!intent) {
    return nullptr;
  }

  auto mojom_intent = apps::mojom::Intent::New();
  mojom_intent->action = intent->action;

  if (intent->url.has_value()) {
    mojom_intent->url = intent->url.value();
  }
  if (intent->mime_type.has_value()) {
    mojom_intent->mime_type = intent->mime_type.value();
  }
  if (!intent->files.empty()) {
    mojom_intent->files = std::vector<apps::mojom::IntentFilePtr>{};
    for (const auto& file : intent->files) {
      mojom_intent->files->push_back(ConvertIntentFileToMojomIntentFile(file));
    }
  }
  if (intent->activity_name.has_value()) {
    mojom_intent->activity_name = intent->activity_name.value();
  }
  if (intent->drive_share_url.has_value()) {
    mojom_intent->drive_share_url = intent->drive_share_url.value();
  }
  if (intent->share_text.has_value()) {
    mojom_intent->share_text = intent->share_text.value();
  }
  if (intent->share_title.has_value()) {
    mojom_intent->share_title = intent->share_title.value();
  }
  if (intent->start_type.has_value()) {
    mojom_intent->start_type = intent->start_type.value();
  }
  if (!intent->categories.empty()) {
    mojom_intent->categories = intent->categories;
  }
  if (intent->data.has_value()) {
    mojom_intent->data = intent->data.value();
  }
  mojom_intent->ui_bypassed = GetMojomOptionalBool(intent->ui_bypassed);
  if (!intent->extras.empty()) {
    mojom_intent->extras = intent->extras;
  }
  return mojom_intent;
}

}  // namespace apps
