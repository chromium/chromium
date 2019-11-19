// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cookies_tree_model_util.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/i18n/time_formatting.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/cache_storage_context.h"
#include "content/public/browser/indexed_db_context.h"
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
    base::DictionaryValue* dict) {
  // Use node's address as an id for WebUI to look it up.
  dict->SetString(kKeyId, GetTreeNodeId(&node));
  dict->SetString(kKeyTitle, node.GetTitle());
  dict->SetBoolean(kKeyHasChildren, !node.children().empty());

  switch (node.GetDetailedInfo().node_type) {
    case CookieTreeNode::DetailedInfo::TYPE_HOST: {
      dict->SetString(kKeyType, "origin");
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_COOKIE: {
      dict->SetString(kKeyType, "cookie");

      const net::CanonicalCookie& cookie = *node.GetDetailedInfo().cookie;

      dict->SetString(kKeyName, cookie.Name());
      dict->SetString(kKeyContent, cookie.Value());
      dict->SetString(kKeyDomain, cookie.Domain());
      dict->SetString(kKeyPath, cookie.Path());
      dict->SetString(kKeySendFor,
                      l10n_util::GetStringUTF16(
                          CookiesTreeModel::GetSendForMessageID(cookie)));
      std::string accessible = cookie.IsHttpOnly() ?
          l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_ACCESSIBLE_TO_SCRIPT_NO) :
          l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_ACCESSIBLE_TO_SCRIPT_YES);
      dict->SetString(kKeyAccessibleToScript, accessible);
      dict->SetString(kKeyCreated, base::UTF16ToUTF8(
          base::TimeFormatFriendlyDateAndTime(cookie.CreationDate())));
      dict->SetString(kKeyExpires, cookie.IsPersistent() ? base::UTF16ToUTF8(
          base::TimeFormatFriendlyDateAndTime(cookie.ExpiryDate())) :
          l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_EXPIRES_SESSION));

      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_DATABASE: {
      dict->SetString(kKeyType, "database");

      const content::StorageUsageInfo& usage_info =
          *node.GetDetailedInfo().usage_info;

      dict->SetString(kKeyOrigin, usage_info.origin.Serialize());
      dict->SetString(kKeySize, ui::FormatBytes(usage_info.total_size_bytes));
      dict->SetString(kKeyModified,
                      base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                          usage_info.last_modified)));

      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE: {
      dict->SetString(kKeyType, "local_storage");

      const content::StorageUsageInfo& local_storage_info =
          *node.GetDetailedInfo().usage_info;

      dict->SetString(kKeyOrigin, local_storage_info.origin.Serialize());
      dict->SetString(kKeySize,
                      ui::FormatBytes(local_storage_info.total_size_bytes));
      dict->SetString(kKeyModified, base::UTF16ToUTF8(
          base::TimeFormatFriendlyDateAndTime(
              local_storage_info.last_modified)));

      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_APPCACHE: {
      dict->SetString(kKeyType, "app_cache");

      const content::StorageUsageInfo& usage_info =
          *node.GetDetailedInfo().usage_info;

      dict->SetString(kKeyOrigin, usage_info.origin.Serialize());
      dict->SetString(kKeySize, ui::FormatBytes(usage_info.total_size_bytes));
      dict->SetString(kKeyModified,
                      base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                          usage_info.last_modified)));
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB: {
      dict->SetString(kKeyType, "indexed_db");

      const content::StorageUsageInfo& usage_info =
          *node.GetDetailedInfo().usage_info;

      dict->SetString(kKeyOrigin, usage_info.origin.Serialize());
      dict->SetString(kKeySize, ui::FormatBytes(usage_info.total_size_bytes));
      dict->SetString(kKeyModified,
                      base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                          usage_info.last_modified)));
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM: {
      dict->SetString(kKeyType, "file_system");

      const BrowsingDataFileSystemHelper::FileSystemInfo& file_system_info =
          *node.GetDetailedInfo().file_system_info;
      const storage::FileSystemType kPerm = storage::kFileSystemTypePersistent;
      const storage::FileSystemType kTemp = storage::kFileSystemTypeTemporary;

      dict->SetString(kKeyOrigin, file_system_info.origin.Serialize());
      dict->SetString(
          kKeyPersistent,
          base::Contains(file_system_info.usage_map, kPerm)
              ? base::UTF16ToUTF8(ui::FormatBytes(
                    file_system_info.usage_map.find(kPerm)->second))
              : l10n_util::GetStringUTF8(IDS_COOKIES_FILE_SYSTEM_USAGE_NONE));
      dict->SetString(
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

      dict->SetString(kKeyType, "quota");

      const BrowsingDataQuotaHelper::QuotaInfo& quota_info =
          *node.GetDetailedInfo().quota_info;
      if (quota_info.temporary_usage + quota_info.persistent_usage <=
          kNegligibleUsage)
        return false;

      dict->SetString(kKeyOrigin, quota_info.host);
      dict->SetString(kKeyTotalUsage,
                      base::UTF16ToUTF8(ui::FormatBytes(
                          quota_info.temporary_usage +
                          quota_info.persistent_usage)));
      dict->SetString(kKeyTemporaryUsage,
                      base::UTF16ToUTF8(ui::FormatBytes(
                          quota_info.temporary_usage)));
      dict->SetString(kKeyPersistentUsage,
                      base::UTF16ToUTF8(ui::FormatBytes(
                          quota_info.persistent_usage)));
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKER: {
      dict->SetString(kKeyType, "service_worker");

      const content::StorageUsageInfo& usage_info =
          *node.GetDetailedInfo().usage_info;

      dict->SetString(kKeyOrigin, usage_info.origin.Serialize());
      dict->SetString(kKeySize, ui::FormatBytes(usage_info.total_size_bytes));
      // TODO(jsbell): Include kKeyModified like other storage types.
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_SHARED_WORKER: {
      dict->SetString(kKeyType, "shared_worker");

      const BrowsingDataSharedWorkerHelper::SharedWorkerInfo&
          shared_worker_info = *node.GetDetailedInfo().shared_worker_info;

      dict->SetString(kKeyOrigin, shared_worker_info.worker.spec());
      dict->SetString(kKeyName, shared_worker_info.name);
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_CACHE_STORAGE: {
      dict->SetString(kKeyType, "cache_storage");

      const content::StorageUsageInfo& usage_info =
          *node.GetDetailedInfo().usage_info;

      dict->SetString(kKeyOrigin, usage_info.origin.Serialize());
      dict->SetString(kKeySize, ui::FormatBytes(usage_info.total_size_bytes));
      dict->SetString(kKeyModified,
                      base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                          usage_info.last_modified)));
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_FLASH_LSO: {
      dict->SetString(kKeyType, "flash_lso");

      dict->SetString(kKeyDomain, node.GetDetailedInfo().flash_lso_domain);
      break;
    }
    case CookieTreeNode::DetailedInfo::TYPE_MEDIA_LICENSE: {
      dict->SetString(kKeyType, "media_license");

      const BrowsingDataMediaLicenseHelper::MediaLicenseInfo&
          media_license_info = *node.GetDetailedInfo().media_license_info;
      dict->SetString(kKeyOrigin, media_license_info.origin.spec());
      dict->SetString(kKeySize, ui::FormatBytes(media_license_info.size));
      dict->SetString(kKeyModified,
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
    auto app_infos = std::make_unique<base::ListValue>();
    for (extensions::ExtensionSet::const_iterator it = protecting_apps->begin();
         it != protecting_apps->end(); ++it) {
      std::unique_ptr<base::DictionaryValue> app_info(
          new base::DictionaryValue());
      app_info->SetString(kKeyId, (*it)->id());
      app_info->SetString(kKeyName, (*it)->name());
      app_infos->Append(std::move(app_info));
    }
    dict->Set(kKeyAppsProtectingThis, std::move(app_infos));
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
      std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
      if (GetCookieTreeNodeDictionary(*details, include_quota_nodes,
                                      dict.get())) {
        // TODO(dschuyler): This ID path is an artifact from using tree nodes to
        // hold the cookies. Can this be changed to a dictionary with a key
        // lookup (and remove use of id_map_)?
        dict->SetString("idPath",
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
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
    const CookieTreeNode* child = parent->children()[start + i].get();
    if (GetCookieTreeNodeDictionary(*child, include_quota_nodes, dict.get()))
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
    const base::string16& title) {
  // TODO(dschuyler): This is an O(n) lookup for O(1) space, but it could be
  // improved to O(1) lookup if desired (by using O(n) space).
  const auto i = std::find_if(
      root->children().cbegin(), root->children().cend(),
      [&title](const auto& child) { return title == child->GetTitle(); });
  return (i == root->children().cend()) ? nullptr : i->get();
}
