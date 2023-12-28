// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/common/intent_helper/arc_intent_helper_mojo_delegate.h"

namespace arc {

ArcIntentHelperMojoDelegate::IntentInfo::IntentInfo(
    std::string action,
    std::optional<std::vector<std::string>> categories,
    std::optional<std::string> data,
    std::optional<std::string> type,
    bool ui_bypassed,
    std::optional<base::flat_map<std::string, std::string>> extras)
    : action(std::move(action)),
      categories(std::move(categories)),
      data(std::move(data)),
      type(std::move(type)),
      ui_bypassed(ui_bypassed),
      extras(std::move(extras)) {}

ArcIntentHelperMojoDelegate::IntentInfo::IntentInfo(const IntentInfo& other) =
    default;

ArcIntentHelperMojoDelegate::IntentInfo::~IntentInfo() = default;

ArcIntentHelperMojoDelegate::TextSelectionAction::TextSelectionAction(
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

ArcIntentHelperMojoDelegate::TextSelectionAction::TextSelectionAction(
    const TextSelectionAction& other) = default;

ArcIntentHelperMojoDelegate::TextSelectionAction::~TextSelectionAction() =
    default;

ArcIntentHelperMojoDelegate::IntentHandlerInfo::IntentHandlerInfo(
    std::string name,
    std::string package_name,
    std::string activity_name,
    bool is_preferred,
    std::optional<std::string> fallback_url)
    : name(std::move(name)),
      package_name(std::move(package_name)),
      activity_name(std::move(activity_name)),
      is_preferred(is_preferred),
      fallback_url(std::move(fallback_url)) {}

ArcIntentHelperMojoDelegate::IntentHandlerInfo::IntentHandlerInfo(
    const IntentHandlerInfo& other) = default;

ArcIntentHelperMojoDelegate::IntentHandlerInfo::~IntentHandlerInfo() = default;

}  // namespace arc
