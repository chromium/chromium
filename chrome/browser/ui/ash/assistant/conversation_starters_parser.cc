// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/conversation_starters_parser.h"

#include <memory>

#include "ash/public/cpp/assistant/conversation_starter.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"

namespace {

// Keys.
constexpr char kActionUrlKey[] = "actionUrl";
constexpr char kConversationStarterKey[] = "conversationStarter";
constexpr char kIconUrlKey[] = "iconUrl";
constexpr char kLabelKey[] = "label";
constexpr char kRequiredPermissionKey[] = "requiredPermission";

// Permissions.
constexpr char kRelatedInfoPermission[] = "RELATED_INFO";

// Helpers ---------------------------------------------------------------------

std::unique_ptr<base::Value> Deserialize(const std::string& safe_json_response,
                                         std::string* error_message) {
  constexpr char kPrefix[] = ")]}'";
  if (!base::StartsWith(safe_json_response, kPrefix,
                        base::CompareCase::SENSITIVE)) {
    return nullptr;
  }
  int error_code = 0;
  return JSONStringValueDeserializer(
             base::MakeStringPiece(
                 safe_json_response.begin() + base::size(kPrefix),
                 safe_json_response.end()))
      .Deserialize(&error_code, error_message);
}

base::Optional<std::string> FindLabel(const base::Value& conversation_starter) {
  const std::string* label = conversation_starter.FindStringKey(kLabelKey);
  return label ? base::Optional<std::string>(*label) : base::nullopt;
}

base::Optional<GURL> FindActionUrl(const base::Value& conversation_starter) {
  const std::string* action_url =
      conversation_starter.FindStringKey(kActionUrlKey);
  return action_url ? base::Optional<GURL>(*action_url) : base::nullopt;
}

base::Optional<GURL> FindIconUrl(const base::Value& conversation_starter) {
  const std::string* icon_url = conversation_starter.FindStringKey(kIconUrlKey);
  return icon_url ? base::Optional<GURL>(*icon_url) : base::nullopt;
}

uint32_t FindRequiredPermissions(const base::Value& conversation_starter) {
  using Permission = ash::ConversationStarter::Permission;

  uint32_t required_permissions = 0u;

  const base::Value* required_permissions_list =
      conversation_starter.FindListKey(kRequiredPermissionKey);
  if (!required_permissions_list)
    return required_permissions;

  for (auto& required_permission : required_permissions_list->GetList()) {
    if (required_permission.GetString() == kRelatedInfoPermission) {
      required_permissions |= Permission::kRelatedInfo;
      continue;
    }
    required_permissions |= Permission::kUnknown;
  }

  return required_permissions;
}

}  // namespace

// ConversationStartersParser --------------------------------------------------

// static
std::vector<ash::ConversationStarter> ConversationStartersParser::Parse(
    const std::string& safe_json_response) {
  std::vector<ash::ConversationStarter> conversation_starters;

  // Deserialize |safe_json_response|.
  std::string error_message;
  auto response = Deserialize(safe_json_response, &error_message);
  if (!response || !response->is_dict() || !error_message.empty()) {
    LOG(ERROR) << (error_message.empty() ? "Error deserializing response."
                                         : error_message);
    return conversation_starters;
  }

  // Parse |response|.
  base::Value* conversation_starters_list =
      response->FindListKey(kConversationStarterKey);
  if (!conversation_starters_list) {
    LOG(ERROR) << "Error parsing response.";
    return conversation_starters;
  }

  // Parse each |conversation_starter|.
  for (auto& conversation_starter : conversation_starters_list->GetList()) {
    // Parse |label|.
    base::Optional<std::string> label = FindLabel(conversation_starter);
    if (!label.has_value()) {
      LOG(ERROR) << "Omitting conversation starter due to missing label.";
      continue;
    }

    // Parse |action_url|, |icon_url|, and |required_permissions|.
    base::Optional<GURL> action_url = FindActionUrl(conversation_starter);
    base::Optional<GURL> icon_url = FindIconUrl(conversation_starter);
    uint32_t required_permissions =
        FindRequiredPermissions(conversation_starter);

    // Add well formed conversation starters to our collection.
    conversation_starters.emplace_back(label.value(), action_url, icon_url,
                                       required_permissions);
  }

  // Return our |conversation_starters| collection.
  // Note that we have already confirmed that all elements are well formed.
  return conversation_starters;
}
