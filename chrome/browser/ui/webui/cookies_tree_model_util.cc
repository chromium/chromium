// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
const char kKeyPersistentUsage[] = "persistentUsage";

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

bool CookiesTreeModelUtil::GetCookieTreeNodeDictionary(
    const CookieTreeNode& node,
    bool include_quota_nodes,
    base::Value* dict) {
  // Use node's address as an id for WebUI to look it up.
  dict->SetStringKey(kKeyId, GetTreeNodeId(&node));
  dict->SetStringKey(kKeyTitle, node.GetTitle());
  dict->SetBoolKey(kKeyHasChildren, !node.children().empty());

  switch (node.GetDetailedInfo().node_type) {
    case CookieTreeNode::DetailedInfo::TYPE_HOST: {
      dict->SetStringKey(kKeyType, "origin");
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_COOKIE: {
      dict->SetStringKey(kKeyType, "cookie");

      const net::CanonicalCookie& cookie = *node.GetDetailedInfo().cookie;

      dict->SetStringKey(kKeyName, cookie.Name());
      dict->SetStringKey(kKeyContent, cookie.Value());
      dict->SetStringKey(kKeyDomain, cookie.Domain());
      dict->SetStringKey(kKeyPath, cookie.Path());
      dict->SetStringKey(kKeySendFor,
                         l10n_util::GetStringUTF16(
                             CookiesTreeModel::GetSendForMessageID(cookie)));
      std::string accessible = cookie.IsHttpOnly() ?
          l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_ACCESSIBLE_TO_SCRIPT_NO) :
          l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_ACCESSIBLE_TO_SCRIPT_YES);
      dict->SetStringKey(kKeyAccessibleToScript, accessible);
      dict->SetStringKey(kKeyCreated,
                         base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                             cookie.CreationDate())));
      dict->SetStringKey(
          kKeyExpires,
          cookie.IsPersistent()
              ? base::UTF16ToUTF8(
                    base::TimeFormatFriendlyDateAndTime(cookie.ExpiryDate()))
              : l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_EXPIRES_SESSION));

      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_DATABASE: {
      dict->SetStringKey(kKeyType, "database");

      const content::StorageUsageInfo& usage_info =
          *node.GetDetailedInfo().usage_info;

      dict->SetStringKey(kKeyOrigin, usage_info.origin.Serialize());
      dict->SetStringKey(kKeySize,
                         ui::FormatBytes(usage_info.total_size_bytes));
      dict->SetStringKey(kKeyModified,
                         base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                             usage_info.last_modified)));

      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE: {
      dict->SetStringKey(kKeyType, "local_storage");

      const content::StorageUsageInfo& local_storage_info =
          *node.GetDetailedInfo().usage_info;

      dict->SetStringKey(kKeyOrigin, local_storage_info.origin.Serialize());
      dict->SetStringKey(kKeySize,
                         ui::FormatBytes(local_storage_info.total_size_bytes));
      dict->SetStringKey(kKeyModified,
                         base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                             local_storage_info.last_modified)));

      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_APPCACHE: {
      dict->SetStringKey(kKeyType, "app_cache");

      const content::StorageUsageInfo& usage_info =
          *node.GetDetailedInfo().usage_info;

      dict->SetStringKey(kKeyOrigin, usage_info.origin.Serialize());
      dict->SetStringKey(kKeySize,
                         ui::FormatBytes(usage_info.total_size_bytes));
      dict->SetStringKey(kKeyModified,
                         base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                             usage_info.last_modified)));
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB: {
      dict->SetStringKey(kKeyType, "indexed_db");

      const content::StorageUsageInfo& usage_info =
          *node.GetDetailedInfo().usage_info;

      dict->SetStringKey(kKeyOrigin, usage_info.origin.Serialize());
      dict->SetStringKey(kKeySize,
                         ui::FormatBytes(usage_info.total_size_bytes));
      dict->SetStringKey(kKeyModified,
                         base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                             usage_info.last_modified)));
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM: {
      dict->SetStringKey(kKeyType, "file_system");

      const browsing_data::FileSystemHelper::FileSystemInfo& file_system_info =
          *node.GetDetailedInfo().file_system_info;
      const storage::FileSystemType kPerm = storage::kFileSystemTypePersistent;
      const storage::FileSystemType kTemp = storage::kFileSystemTypeTemporary;

      dict->SetStringKey(kKeyOrigin, file_system_info.origin.Serialize());
      dict->SetStringKey(
          kKeyPersistent,
          base::Contains(file_system_info.usage_map, kPerm)
              ? base::UTF16ToUTF8(ui::FormatBytes(
                    file_system_info.usage_map.find(kPerm)->second))
              : l10n_util::GetStringUTF8(IDS_COOKIES_FILE_SYSTEM_USAGE_NONE));
      dict->SetStringKey(
          kKeyTemporary,
          base::Contains(file_system_info.usage_map, kTemp)
              ? base::UTF16ToUTF8(ui::FormatBytes(
                    file_system_info.usage_map.find(kTemp)->second))
              : l10n_util::GetStringUTF8(IDS_COOKIES_FILE_SYSTEM_USAGE_NONE));
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_QUOTA: {
      if (!include_quota_nodes)
        return false;

      dict->SetStringKey(kKeyType, "quota");

      const BrowsingDataQuotaHelper::QuotaInfo& quota_info =
          *node.GetDetailedInfo().quota_info;
      if (quota_info.temporary_usage + quota_info.persistent_usage <=
          kNegligibleUsage)
        return false;

      dict->SetStringKey(kKeyOrigin, quota_info.host);
      dict->SetStringKey(
          kKeyTotalUsage,
          base::UTF16ToUTF8(ui::FormatBytes(quota_info.temporary_usage +
                                            quota_info.persistent_usage)));
      dict->SetStringKey(
          kKeyTemporaryUsage,
          base::UTF16ToUTF8(ui::FormatBytes(quota_info.temporary_usage)));
      dict->SetStringKey(
          kKeyPersistentUsage,
          base::UTF16ToUTF8(ui::FormatBytes(quota_info.persistent_usage)));
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKER: {
      dict->SetStringKey(kKeyType, "service_worker");

      const content::StorageUsageInfo& usage_info =
          *node.GetDetailedInfo().usage_info;

      dict->SetStringKey(kKeyOrigin, usage_info.origin.Serialize());
      dict->SetStringKey(kKeySize,
                         ui::FormatBytes(usage_info.total_size_bytes));
      // TODO(jsbell): Include kKeyModified like other storage types.
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_SHARED_WORKER: {
      dict->SetStringKey(kKeyType, "shared_worker");

      const browsing_data::SharedWorkerHelper::SharedWorkerInfo&
          shared_worker_info = *node.GetDetailedInfo().shared_worker_info;

      dict->SetStringKey(kKeyOrigin, shared_worker_info.worker.spec());
      dict->SetStringKey(kKeyName, shared_worker_info.name);
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_CACHE_STORAGE: {
      dict->SetStringKey(kKeyType, "cache_storage");

      const content::StorageUsageInfo& usage_info =
          *node.GetDetailedInfo().usage_info;

      dict->SetStringKey(kKeyOrigin, usage_info.origin.Serialize());
      dict->SetStringKey(kKeySize,
                         ui::FormatBytes(usage_info.total_size_bytes));
      dict->SetStringKey(kKeyModified,
                         base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                             usage_info.last_modified)));
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_MEDIA_LICENSE: {
      dict->SetStringKey(kKeyType, "media_license");

      const BrowsingDataMediaLicenseHelper::MediaLicenseInfo&
          media_license_info = *node.GetDetailedInfo().media_license_info;
      dict->SetStringKey(kKeyOrigin, media_license_info.origin.spec());
      dict->SetStringKey(kKeySize, ui::FormatBytes(media_license_info.size));
      dict->SetStringKey(kKeyModified,
                         base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                             media_license_info.last_modified_time)));
      break;
    }
    default:
      break;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  const extensions::ExtensionSet* protecting_apps =
      node.GetModel()->ExtensionsProtectingNode(node);
  if (protecting_apps && !protecting_apps->is_empty()) {
    base::ListValue app_infos;
    for (extensions::ExtensionSet::const_iterator it = protecting_apps->begin();
         it != protecting_apps->end(); ++it) {
      base::Value app_info(base::Value::Type::DICTIONARY);
      app_info.SetStringKey(kKeyId, (*it)->id());
      app_info.SetStringKey(kKeyName, (*it)->name());
      app_infos.Append(std::move(app_info));
    }
    dict->SetKey(kKeyAppsProtectingThis, std::move(app_infos));
  }
#endif

  return true;
}

void CookiesTreeModelUtil::GetChildNodeDetails(const CookieTreeNode* parent,
                                               bool include_quota_nodes,
                                               base::ListValue* list) {
  std::string id_path = GetTreeNodeId(parent);
  for (const auto& child : parent->children()) {
    std::string cookie_id_path =
        id_path + "," + GetTreeNodeId(child.get()) + ",";
    for (const auto& details : child->children()) {
      base::Value dict(base::Value::Type::DICTIONARY);
      if (GetCookieTreeNodeDictionary(*details, include_quota_nodes, &dict)) {
        // TODO(dschuyler): This ID path is an artifact from using tree nodes to
        // hold the cookies. Can this be changed to a dictionary with a key
        // lookup (and remove use of id_map_)?
        dict.SetStringKey("idPath",
                          cookie_id_path + GetTreeNodeId(details.get()));
        list->Append(std::move(dict));
      }
    }
  }
}

void CookiesTreeModelUtil::GetChildNodeList(const CookieTreeNode* parent,
                                            size_t start,
                                            size_t count,
                                            bool include_quota_nodes,
                                            base::ListValue* nodes) {
  for (size_t i = 0; i < count; ++i) {
    base::Value dict(base::Value::Type::DICTIONARY);
    const CookieTreeNode* child = parent->children()[start + i].get();
    if (GetCookieTreeNodeDictionary(*child, include_quota_nodes, &dict))
      nodes->Append(std::move(dict));
  }
}

const CookieTreeNode* CookiesTreeModelUtil::GetTreeNodeFromPath(
    const CookieTreeNode* root,
    const std::string& path) {
  const CookieTreeNode* child = NULL;
  const CookieTreeNode* parent = root;
  int child_index = -1;

  // Validate the tree path and get the node pointer.
  for (const base::StringPiece& cur_node : base::SplitStringPiece(
           path, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    int32_t node_id = 0;
    if (!base::StringToInt(cur_node, &node_id))
      break;

    child = id_map_.Lookup(node_id);
    child_index = parent->GetIndexOf(child);
    if (child_index == -1)
      break;

    parent = child;
  }

  return child_index >= 0 ? child : NULL;
}

const CookieTreeNode* CookiesTreeModelUtil::GetTreeNodeFromTitle(
    const CookieTreeNode* root,
    const std::u16string& title) {
  // TODO(dschuyler): This is an O(n) lookup for O(1) space, but it could be
  // improved to O(1) lookup if desired (by using O(n) space).
  const auto i = std::find_if(
      root->children().cbegin(), root->children().cend(),
      [&title](const auto& child) { return title == child->GetTitle(); });
  return (i == root->children().cend()) ? nullptr : i->get();
}
