// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cookies_tree_model_util.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_usage_info.h"
#include "extensions/buildflags/buildflags.h"
#include "net/cookies/canonical_cookie.h"
#include "storage/common/file_system/file_system_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_set.h"
#endif

namespace {

const char kKeyId[] = "id";
const char kKeyTitle[] = "title";
const char kKeyType[] = "type";
const char kKeyHasChildren[] = "hasChildren";

#if BUILDFLAG(ENABLE_EXTENSIONS)
const char kKeyAppsProtectingThis[] = "appsProtectingThis";
#endif
const char kKeyName[] = "name";
const char kKeyContent[] = "content";
const char kKeyDomain[] = "domain";
const char kKeyPath[] = "path";
const char kKeySendFor[] = "sendfor";
const char kKeyAccessibleToScript[] = "accessibleToScript";
const char kKeySize[] = "size";
const char kKeyOrigin[] = "origin";
const char kKeyCreated[] = "created";
const char kKeyExpires[] = "expires";
const char kKeyModified[] = "modified";

const char kKeyPersistent[] = "persistent";
const char kKeyTemporary[] = "temporary";

const char kKeyTotalUsage[] = "totalUsage";
const char kKeyTemporaryUsage[] = "temporaryUsage";

const int64_t kNegligibleUsage = 1024;  // 1KiB

}  // namespace

CookiesTreeModelUtil::CookiesTreeModelUtil() {
}

CookiesTreeModelUtil::~CookiesTreeModelUtil() {
}

std::string CookiesTreeModelUtil::GetTreeNodeId(const CookieTreeNode* node) {
  CookieTreeNodeMap::const_iterator iter = node_map_.find(node);
  if (iter != node_map_.end())
    return base::NumberToString(iter->second);

  int32_t new_id = id_map_.Add(node);
  node_map_[node] = new_id;
  return base::NumberToString(new_id);
}

absl::optional<base::Value::Dict>
CookiesTreeModelUtil::GetCookieTreeNodeDictionary(const CookieTreeNode& node) {
  base::Value::Dict dict;

  switch (node.GetDetailedInfo().node_type) {
    case CookieTreeNode::DetailedInfo::TYPE_HOST: {
      dict.Set(kKeyType, "origin");
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_COOKIE: {
      dict.Set(kKeyType, "cookie");

      const net::CanonicalCookie& cookie = *node.GetDetailedInfo().cookie;

      dict.Set(kKeyName, cookie.Name());
      dict.Set(kKeyContent, cookie.Value());
      dict.Set(kKeyDomain, cookie.Domain());
      dict.Set(kKeyPath, cookie.Path());
      dict.Set(kKeySendFor, l10n_util::GetStringUTF16(
                                CookiesTreeModel::GetSendForMessageID(cookie)));
      std::string accessible = cookie.IsHttpOnly() ?
          l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_ACCESSIBLE_TO_SCRIPT_NO) :
          l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_ACCESSIBLE_TO_SCRIPT_YES);
      dict.Set(kKeyAccessibleToScript, accessible);
      dict.Set(kKeyCreated,
               base::TimeFormatFriendlyDateAndTime(cookie.CreationDate()));
      dict.Set(
          kKeyExpires,
          cookie.IsPersistent()
              ? base::TimeFormatFriendlyDateAndTime(cookie.ExpiryDate())
              : l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_EXPIRES_SESSION));

      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_DATABASE: {
      dict.Set(kKeyType, "database");

      const content::StorageUsageInfo& usage_info =
          *node.GetDetailedInfo().usage_info;

      dict.Set(kKeyOrigin, usage_info.storage_key.origin().Serialize());
      dict.Set(kKeySize, ui::FormatBytes(usage_info.total_size_bytes));
      dict.Set(kKeyModified,
               base::TimeFormatFriendlyDateAndTime(usage_info.last_modified));
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE: {
      dict.Set(kKeyType, "local_storage");

      const content::StorageUsageInfo& local_storage_info =
          *node.GetDetailedInfo().usage_info;

      dict.Set(kKeyOrigin, local_storage_info.storage_key.origin().Serialize());
      dict.Set(kKeySize, ui::FormatBytes(local_storage_info.total_size_bytes));
      dict.Set(kKeyModified, base::TimeFormatFriendlyDateAndTime(
                                 local_storage_info.last_modified));

      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB: {
      dict.Set(kKeyType, "indexed_db");

      const content::StorageUsageInfo& usage_info =
          *node.GetDetailedInfo().usage_info;

      dict.Set(kKeyOrigin, usage_info.storage_key.origin().Serialize());
      dict.Set(kKeySize, ui::FormatBytes(usage_info.total_size_bytes));
      dict.Set(kKeyModified,
               base::TimeFormatFriendlyDateAndTime(usage_info.last_modified));
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM: {
      dict.Set(kKeyType, "file_system");

      const browsing_data::FileSystemHelper::FileSystemInfo& file_system_info =
          *node.GetDetailedInfo().file_system_info;
      const storage::FileSystemType kPerm = storage::kFileSystemTypePersistent;
      const storage::FileSystemType kTemp = storage::kFileSystemTypeTemporary;

      dict.Set(kKeyOrigin, file_system_info.origin.Serialize());
      dict.Set(
          kKeyPersistent,
          base::Contains(file_system_info.usage_map, kPerm)
              ? ui::FormatBytes(file_system_info.usage_map.find(kPerm)->second)
              : l10n_util::GetStringUTF16(IDS_COOKIES_FILE_SYSTEM_USAGE_NONE));
      dict.Set(
          kKeyTemporary,
          base::Contains(file_system_info.usage_map, kTemp)
              ? ui::FormatBytes(file_system_info.usage_map.find(kTemp)->second)
              : l10n_util::GetStringUTF16(IDS_COOKIES_FILE_SYSTEM_USAGE_NONE));
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_QUOTA: {
      dict.Set(kKeyType, "quota");

      const BrowsingDataQuotaHelper::QuotaInfo& quota_info =
          *node.GetDetailedInfo().quota_info;
      if (quota_info.temporary_usage <= kNegligibleUsage)
        return absl::nullopt;

      dict.Set(kKeyOrigin, quota_info.storage_key.origin().host());
      dict.Set(kKeyTotalUsage, ui::FormatBytes(quota_info.temporary_usage));
      dict.Set(kKeyTemporaryUsage, ui::FormatBytes(quota_info.temporary_usage));
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKER: {
      dict.Set(kKeyType, "service_worker");

      const content::StorageUsageInfo& usage_info =
          *node.GetDetailedInfo().usage_info;

      dict.Set(kKeyOrigin, usage_info.storage_key.origin().Serialize());
      dict.Set(kKeySize, ui::FormatBytes(usage_info.total_size_bytes));
      // TODO(jsbell): Include kKeyModified like other storage types.
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_SHARED_WORKER: {
      dict.Set(kKeyType, "shared_worker");

      const browsing_data::SharedWorkerInfo& shared_worker_info =
          *node.GetDetailedInfo().shared_worker_info;

      dict.Set(kKeyOrigin, shared_worker_info.worker.spec());
      dict.Set(kKeyName, shared_worker_info.name);
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_CACHE_STORAGE: {
      dict.Set(kKeyType, "cache_storage");

      const content::StorageUsageInfo& usage_info =
          *node.GetDetailedInfo().usage_info;

      dict.Set(kKeyOrigin, usage_info.storage_key.origin().Serialize());
      dict.Set(kKeySize, ui::FormatBytes(usage_info.total_size_bytes));
      dict.Set(kKeyModified,
               base::TimeFormatFriendlyDateAndTime(usage_info.last_modified));
      break;
    }
    default:
      break;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  const extensions::ExtensionSet* protecting_apps =
      node.GetModel()->ExtensionsProtectingNode(node);
  if (protecting_apps && !protecting_apps->empty()) {
    base::Value::List app_infos;
    for (const auto& app : *protecting_apps) {
      base::Value::Dict app_info;
      app_info.Set(kKeyId, app->id());
      app_info.Set(kKeyName, app->name());
      app_infos.Append(std::move(app_info));
    }
    dict.Set(kKeyAppsProtectingThis, std::move(app_infos));
  }
#endif

  // Only node types with detailed information above result in a dict.
  if (dict.empty())
    return absl::nullopt;

  dict.Set(kKeyId, GetTreeNodeId(&node));
  dict.Set(kKeyTitle, node.GetTitle());
  dict.Set(kKeyHasChildren, !node.children().empty());

  return dict;
}

base::Value::List CookiesTreeModelUtil::GetChildNodeDetailsDeprecated(
    const CookieTreeNode* parent) {
  base::Value::List list;
  std::string id_path = GetTreeNodeId(parent);
  for (const auto& child : parent->children()) {
    // Node types of interest either live at this level, or the level below.
    // Whether a node is of interest is determined by
    // GetCookieTreeNodeDictionary().
    std::string cookie_id_path = id_path + "," + GetTreeNodeId(child.get());
    absl::optional<base::Value::Dict> child_dict =
        GetCookieTreeNodeDictionary(*child);
    if (child_dict) {
      child_dict->Set("idPath", cookie_id_path);
      list.Append(std::move(*child_dict));
    }
    cookie_id_path += ",";

    for (const auto& details : child->children()) {
      absl::optional<base::Value::Dict> details_dict =
          GetCookieTreeNodeDictionary(*details);
      if (details_dict) {
        // TODO(dschuyler): This ID path is an artifact from using tree nodes to
        // hold the cookies. Can this be changed to a dictionary with a key
        // lookup (and remove use of id_map_)?
        details_dict->Set("idPath",
                          cookie_id_path + GetTreeNodeId(details.get()));
        list.Append(std::move(*details_dict));
      }
    }
  }
  return list;
}

const CookieTreeNode* CookiesTreeModelUtil::GetTreeNodeFromPath(
    const CookieTreeNode* root,
    const std::string& path) {
  const CookieTreeNode* child = nullptr;
  const CookieTreeNode* parent = root;
  absl::optional<size_t> child_index;

  // Validate the tree path and get the node pointer.
  for (const base::StringPiece& cur_node : base::SplitStringPiece(
           path, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    int32_t node_id = 0;
    if (!base::StringToInt(cur_node, &node_id))
      break;

    child = id_map_.Lookup(node_id);
    child_index = parent->GetIndexOf(child);
    if (!child_index.has_value())
      break;

    parent = child;
  }

  return child_index.has_value() ? child : nullptr;
}

const CookieTreeNode* CookiesTreeModelUtil::GetTreeNodeFromTitle(
    const CookieTreeNode* root,
    const std::u16string& title) {
  // TODO(dschuyler): This is an O(n) lookup for O(1) space, but it could be
  // improved to O(1) lookup if desired (by using O(n) space).
  const auto i =
      base::ranges::find(root->children(), title, &CookieTreeNode::GetTitle);
  return (i == root->children().cend()) ? nullptr : i->get();
}
