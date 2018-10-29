// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"

#include <vector>

#include "base/metrics/field_trial.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/permissions/permission_message_util.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// Copyable wrapper to make PermissionMessages comparable.
class ComparablePermission {
 public:
  explicit ComparablePermission(const PermissionMessage& msg) : msg_(&msg) {}

  bool operator<(const ComparablePermission& rhs) const {
    if (msg_->message() < rhs.msg_->message())
      return true;
    if (msg_->message() > rhs.msg_->message())
      return false;
    return msg_->submessages() < rhs.msg_->submessages();
  }

  bool operator==(const ComparablePermission& rhs) const {
    return msg_->message() == rhs.msg_->message() &&
           msg_->submessages() == rhs.msg_->submessages();
  }

 private:
  const PermissionMessage* msg_;
};
using ComparablePermissions = std::vector<ComparablePermission>;

}  // namespace

typedef std::set<PermissionMessage> PermissionMsgSet;

ChromePermissionMessageProvider::ChromePermissionMessageProvider() {
}

ChromePermissionMessageProvider::~ChromePermissionMessageProvider() {
}

PermissionMessages ChromePermissionMessageProvider::GetPermissionMessages(
    const PermissionIDSet& permissions) const {
  const std::vector<ChromePermissionMessageRule> rules =
      ChromePermissionMessageRule::GetAllRules();

  return GetPermissionMessagesHelper(permissions, rules);
}

bool ChromePermissionMessageProvider::IsPrivilegeIncrease(
    const PermissionSet& granted_permissions,
    const PermissionSet& requested_permissions,
    Manifest::Type extension_type) const {
  if (IsHostPrivilegeIncrease(granted_permissions, requested_permissions,
                              extension_type))
    return true;

  if (IsAPIOrManifestPrivilegeIncrease(granted_permissions,
                                       requested_permissions))
    return true;

  return false;
}

PermissionIDSet ChromePermissionMessageProvider::GetAllPermissionIDs(
    const PermissionSet& permissions,
    Manifest::Type extension_type) const {
  PermissionIDSet permission_ids;
  AddAPIPermissions(permissions, &permission_ids);
  AddManifestPermissions(permissions, &permission_ids);
  AddHostPermissions(permissions, &permission_ids, extension_type);
  return permission_ids;
}

PermissionMessages
ChromePermissionMessageProvider::GetPowerfulPermissionMessages(
    const PermissionIDSet& permissions) const {
  std::vector<ChromePermissionMessageRule> all_rules =
      ChromePermissionMessageRule::GetAllRules();

  // TODO(crbug.com/888981): Find a better way to get wanted rules. Maybe add a
  // bool to each one telling if we should consider it here or not.
  constexpr size_t rules_considered = 15;
  const std::vector<extensions::ChromePermissionMessageRule> rules(
      all_rules.begin(),
      all_rules.begin() + std::min(rules_considered, all_rules.size()));

  return GetPermissionMessagesHelper(permissions, rules);
}

void ChromePermissionMessageProvider::AddAPIPermissions(
    const PermissionSet& permissions,
    PermissionIDSet* permission_ids) const {
  for (const APIPermission* permission : permissions.apis())
    permission_ids->InsertAll(permission->GetPermissions());

  // A special hack: The warning message for declarativeWebRequest
  // permissions speaks about blocking parts of pages, which is a
  // subset of what the "<all_urls>" access allows. Therefore we
  // display only the "<all_urls>" warning message if both permissions
  // are required.
  // TODO(treib): The same should apply to other permissions that are implied by
  // "<all_urls>" (aka APIPermission::kHostsAll), such as kTab. This would
  // happen automatically if we didn't differentiate between API/Manifest/Host
  // permissions here.
  if (permissions.ShouldWarnAllHosts())
    permission_ids->erase(APIPermission::kDeclarativeWebRequest);
}

void ChromePermissionMessageProvider::AddManifestPermissions(
    const PermissionSet& permissions,
    PermissionIDSet* permission_ids) const {
  for (const ManifestPermission* p : permissions.manifest_permissions())
    permission_ids->InsertAll(p->GetPermissions());
}

void ChromePermissionMessageProvider::AddHostPermissions(
    const PermissionSet& permissions,
    PermissionIDSet* permission_ids,
    Manifest::Type extension_type) const {
  // Since platform apps always use isolated storage, they can't (silently)
  // access user data on other domains, so there's no need to prompt.
  // Note: this must remain consistent with IsHostPrivilegeIncrease.
  // See crbug.com/255229.
  if (extension_type == Manifest::TYPE_PLATFORM_APP)
    return;

  if (permissions.ShouldWarnAllHosts()) {
    permission_ids->insert(APIPermission::kHostsAll);
  } else {
    URLPatternSet regular_hosts;
    ExtensionsClient::Get()->FilterHostPermissions(
        permissions.effective_hosts(), &regular_hosts, permission_ids);

    std::set<std::string> hosts =
        permission_message_util::GetDistinctHosts(regular_hosts, true, true);
    for (const auto& host : hosts) {
      permission_ids->insert(APIPermission::kHostReadWrite,
                             base::UTF8ToUTF16(host));
    }
  }
}

bool ChromePermissionMessageProvider::IsAPIOrManifestPrivilegeIncrease(
    const PermissionSet& granted_permissions,
    const PermissionSet& requested_permissions) const {
  PermissionIDSet granted_ids;
  AddAPIPermissions(granted_permissions, &granted_ids);
  AddManifestPermissions(granted_permissions, &granted_ids);

  // We compare |granted_ids| against the set of permissions that would be
  // granted if the requested permissions are allowed.
  PermissionIDSet potential_total_ids = granted_ids;
  AddAPIPermissions(requested_permissions, &potential_total_ids);
  AddManifestPermissions(requested_permissions, &potential_total_ids);

  // For M62, we added a new permission ID for new tab page overrides. Consider
  // the addition of this permission to not result in a privilege increase for
  // the time being.
  // TODO(robertshield): Remove this once most of the population is on M62+
  granted_ids.erase(APIPermission::kNewTabPageOverride);
  potential_total_ids.erase(APIPermission::kNewTabPageOverride);

  // If all the IDs were already there, it's not a privilege increase.
  if (granted_ids.Includes(potential_total_ids))
    return false;

  // Otherwise, check the actual messages - not all IDs result in a message,
  // and some messages can suppress others.
  PermissionMessages granted_messages = GetPermissionMessages(granted_ids);
  PermissionMessages total_messages =
      GetPermissionMessages(potential_total_ids);

  ComparablePermissions granted_strings(granted_messages.begin(),
                                        granted_messages.end());
  ComparablePermissions total_strings(total_messages.begin(),
                                      total_messages.end());

  std::sort(granted_strings.begin(), granted_strings.end());
  std::sort(total_strings.begin(), total_strings.end());

  // TODO(devlin): I *think* we can just use strict-equals here, since we should
  // never have more strings in granted than in total (unless there was a
  // significant difference - e.g., going from two lower warnings to a single
  // scarier warning because of adding a new permission). But let's be overly
  // conservative for now.
  return !base::STLIncludes(granted_strings, total_strings);
}

bool ChromePermissionMessageProvider::IsHostPrivilegeIncrease(
    const PermissionSet& granted_permissions,
    const PermissionSet& requested_permissions,
    Manifest::Type extension_type) const {
  // Platform apps host permission changes do not count as privilege increases.
  // Note: this must remain consistent with AddHostPermissions.
  if (extension_type == Manifest::TYPE_PLATFORM_APP)
    return false;

  // If the granted permission set can access any host, then it can't be
  // elevated.
  if (granted_permissions.HasEffectiveAccessToAllHosts())
    return false;

  // Likewise, if the requested permission set has full host access, then it
  // must be a privilege increase.
  if (requested_permissions.HasEffectiveAccessToAllHosts())
    return true;

  const URLPatternSet& granted_list = granted_permissions.effective_hosts();
  const URLPatternSet& requested_list = requested_permissions.effective_hosts();

  std::set<std::string> requested_hosts_set(
      permission_message_util::GetDistinctHosts(requested_list, false, false));
  std::set<std::string> granted_hosts_set(
      permission_message_util::GetDistinctHosts(granted_list, false, false));
  std::set<std::string> requested_hosts_only =
      base::STLSetDifference<std::set<std::string>>(requested_hosts_set,
                                                    granted_hosts_set);

  // Try to match any domain permissions against existing domain permissions
  // that overlap, so that migrating from *.example.com -> foo.example.com
  // does not constitute a permissions increase, even though the strings are
  // not exactly the same.
  for (const auto& requested : requested_hosts_only) {
    bool host_matched = false;
    const base::StringPiece unmatched(requested);
    for (const auto& granted : granted_hosts_set) {
      if (granted.size() > 2 && granted[0] == '*' && granted[1] == '.') {
        const base::StringPiece stripped_granted(granted.data() + 1,
                                                 granted.length() - 1);
        // If the unmatched host ends with the the granted host,
        // after removing the '*', then it's a match. In addition,
        // because we consider having access to "*.domain.com" as
        // granting access to "domain.com" then compare the string
        // with both the "*" and the "." removed.
        if (unmatched.ends_with(stripped_granted) ||
            unmatched == stripped_granted.substr(1)) {
          host_matched = true;
          break;
        }
      }
    }
    if (!host_matched) {
      return true;
    }
  }
  return false;
}

PermissionMessages ChromePermissionMessageProvider::GetPermissionMessagesHelper(
    const PermissionIDSet& permissions,
    const std::vector<ChromePermissionMessageRule>& rules) const {
  // Apply each of the rules, in order, to generate the messages for the given
  // permissions. Once a permission is used in a rule, remove it from the set
  // of available permissions so it cannot be applied to subsequent rules.
  PermissionIDSet remaining_permissions = permissions;
  PermissionMessages messages;
  for (const auto& rule : rules) {
    // Only apply the rule if we have all the required permission IDs.
    if (remaining_permissions.ContainsAllIDs(rule.required_permissions())) {
      // We can apply the rule. Add all the required permissions, and as many
      // optional permissions as we can, to the new message.
      PermissionIDSet used_permissions =
          remaining_permissions.GetAllPermissionsWithIDs(
              rule.all_permissions());
      messages.push_back(rule.GetPermissionMessage(used_permissions));

      remaining_permissions =
          PermissionIDSet::Difference(remaining_permissions, used_permissions);
    }
  }

  return messages;
}

}  // namespace extensions
