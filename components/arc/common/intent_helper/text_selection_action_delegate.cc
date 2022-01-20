// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/common/intent_helper/text_selection_action_delegate.h"

namespace arc {

TextSelectionActionDelegate::ActivityName::ActivityName(
    std::string package_name,
    absl::optional<std::string> activity_name)
    : package_name(std::move(package_name)),
      activity_name(std::move(activity_name)) {}

TextSelectionActionDelegate::ActivityName::ActivityName(
    const ActivityName& other) = default;

TextSelectionActionDelegate::ActivityName::~ActivityName() = default;

TextSelectionActionDelegate::IntentInfo::IntentInfo(
    std::string action,
    absl::optional<std::vector<std::string>> categories,
    absl::optional<std::string> data,
    absl::optional<std::string> type,
    bool ui_bypassed,
    absl::optional<base::flat_map<std::string, std::string>> extras)
    : action(std::move(action)),
      categories(std::move(categories)),
      data(std::move(data)),
      type(std::move(type)),
      ui_bypassed(ui_bypassed),
      extras(std::move(extras)) {}

TextSelectionActionDelegate::IntentInfo::IntentInfo(const IntentInfo& other) =
    default;

TextSelectionActionDelegate::IntentInfo::~IntentInfo() = default;

TextSelectionActionDelegate::TextSelectionAction::TextSelectionAction(
    std::string app_id,
    gfx::ImageSkia icon,
    ActivityName activity,
    std::string title,
    IntentInfo action_intent)
    : app_id(std::move(app_id)),
      icon(std::move(icon)),
      activity(std::move(activity)),
      title(std::move(title)),
      action_intent(std::move(action_intent)) {}

TextSelectionActionDelegate::TextSelectionAction::TextSelectionAction(
    const TextSelectionAction& other) = default;

TextSelectionActionDelegate::TextSelectionAction::~TextSelectionAction() =
    default;

}  // namespace arc
