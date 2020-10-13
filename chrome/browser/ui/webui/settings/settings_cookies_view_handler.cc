// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_cookies_view_handler.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/number_formatting.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/cookies_tree_model_util.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/same_site_data_remover.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace storage {
class FileSystemContext;
}

namespace {

int GetCategoryLabelID(CookieTreeNode::DetailedInfo::NodeType node_type) {
  constexpr struct {
    CookieTreeNode::DetailedInfo::NodeType node_type;
    int id;
  } kCategoryLabels[] = {
      // Multiple keys (node_type) may have the same value (id).

      {CookieTreeNode::DetailedInfo::TYPE_DATABASES,
       IDS_SETTINGS_COOKIES_DATABASE_STORAGE},
      {CookieTreeNode::DetailedInfo::TYPE_DATABASE,
       IDS_SETTINGS_COOKIES_DATABASE_STORAGE},

      {CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGES,
       IDS_SETTINGS_COOKIES_LOCAL_STORAGE},
      {CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE,
       IDS_SETTINGS_COOKIES_LOCAL_STORAGE},

      {CookieTreeNode::DetailedInfo::TYPE_APPCACHES,
       IDS_SETTINGS_COOKIES_APPLICATION_CACHE},
      {CookieTreeNode::DetailedInfo::TYPE_APPCACHE,
       IDS_SETTINGS_COOKIES_APPLICATION_CACHE},

      {CookieTreeNode::DetailedInfo::TYPE_INDEXED_DBS,
       IDS_SETTINGS_COOKIES_DATABASE_STORAGE},
      {CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB,
       IDS_SETTINGS_COOKIES_DATABASE_STORAGE},

      {CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEMS,
       IDS_SETTINGS_COOKIES_FILE_SYSTEM},
      {CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM,
       IDS_SETTINGS_COOKIES_FILE_SYSTEM},

      {CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKERS,
       IDS_SETTINGS_COOKIES_SERVICE_WORKER},
      {CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKER,
       IDS_SETTINGS_COOKIES_SERVICE_WORKER},

      {CookieTreeNode::DetailedInfo::TYPE_SHARED_WORKERS,
       IDS_SETTINGS_COOKIES_SHARED_WORKER},
      {CookieTreeNode::DetailedInfo::TYPE_SHARED_WORKER,
       IDS_SETTINGS_COOKIES_SHARED_WORKER},

      {CookieTreeNode::DetailedInfo::TYPE_CACHE_STORAGES,
       IDS_SETTINGS_COOKIES_CACHE_STORAGE},
      {CookieTreeNode::DetailedInfo::TYPE_CACHE_STORAGE,
       IDS_SETTINGS_COOKIES_CACHE_STORAGE},

      {CookieTreeNode::DetailedInfo::TYPE_FLASH_LSO,
       IDS_SETTINGS_COOKIES_FLASH_LSO},

      {CookieTreeNode::DetailedInfo::TYPE_MEDIA_LICENSES,
       IDS_SETTINGS_COOKIES_MEDIA_LICENSE},
      {CookieTreeNode::DetailedInfo::TYPE_MEDIA_LICENSE,
       IDS_SETTINGS_COOKIES_MEDIA_LICENSE},
  };
  // Before optimizing, consider the data size and the cost of L2 cache misses.
  // A linear search over a couple dozen integers is very fast.
  for (size_t i = 0; i < base::size(kCategoryLabels); ++i) {
    if (kCategoryLabels[i].node_type == node_type) {
      return kCategoryLabels[i].id;
    }
  }
  NOTREACHED();
  return 0;
}

}  // namespace

namespace settings {

constexpr char kChildren[] = "children";
constexpr char kCount[] = "count";
constexpr char kId[] = "id";
constexpr char kItems[] = "items";
constexpr char kStart[] = "start";
constexpr char kLocalData[] = "localData";
constexpr char kSite[] = "site";
constexpr char kTotal[] = "total";

CookiesViewHandler::Request::Request() {
  Clear();
}

void CookiesViewHandler::Request::Clear() {
  should_send_list = false;
  callback_id_.clear();
}

CookiesViewHandler::CookiesViewHandler()
    : batch_update_(false), model_util_(new CookiesTreeModelUtil) {}

CookiesViewHandler::~CookiesViewHandler() {
}

void CookiesViewHandler::OnJavascriptAllowed() {
}

void CookiesViewHandler::OnJavascriptDisallowed() {
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
  request_.Clear();
}

void CookiesViewHandler::RegisterMessages() {
  EnsureCookiesTreeModelCreated();

  web_ui()->RegisterMessageCallback(
      "localData.getDisplayList",
      base::BindRepeating(&CookiesViewHandler::HandleGetDisplayList,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "localData.removeAll",
      base::BindRepeating(&CookiesViewHandler::HandleRemoveAll,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "localData.removeShownItems",
      base::BindRepeating(&CookiesViewHandler::HandleRemoveShownItems,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "localData.removeItem",
      base::BindRepeating(&CookiesViewHandler::HandleRemoveItem,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "localData.getCookieDetails",
      base::BindRepeating(&CookiesViewHandler::HandleGetCookieDetails,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "localData.getNumCookiesString",
      base::BindRepeating(&CookiesViewHandler::HandleGetNumCookiesString,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "localData.removeCookie",
      base::BindRepeating(&CookiesViewHandler::HandleRemove,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "localData.removeThirdPartyCookies",
      base::BindRepeating(&CookiesViewHandler::HandleRemoveThirdParty,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "localData.reload",
      base::BindRepeating(&CookiesViewHandler::HandleReloadCookies,
                          base::Unretained(this)));
}

void CookiesViewHandler::TreeNodesAdded(ui::TreeModel* model,
                                        ui::TreeModelNode* parent,
                                        size_t start,
                                        size_t count) {
  // Skip if there is a batch update in progress.
  if (batch_update_)
    return;

  CookiesTreeModel* tree_model = static_cast<CookiesTreeModel*>(model);
  CookieTreeNode* parent_node = tree_model->AsNode(parent);

  std::unique_ptr<base::ListValue> children(new base::ListValue);
  // Passing false for |include_quota_nodes| since they don't reflect reality
  // until bug http://crbug.com/642955 is fixed and local/session storage is
  // counted against the total.
  model_util_->GetChildNodeList(
      parent_node, start, count, /*include_quota_nodes=*/false, children.get());

  base::DictionaryValue args;
  if (parent == tree_model->GetRoot())
    args.Set(kId, std::make_unique<base::Value>());
  else
    args.SetString(kId, model_util_->GetTreeNodeId(parent_node));
  args.SetInteger(kStart, int{start});
  args.Set(kChildren, std::move(children));
  FireWebUIListener("on-tree-item-added", args);
}

void CookiesViewHandler::TreeNodesRemoved(ui::TreeModel* model,
                                          ui::TreeModelNode* parent,
                                          size_t start,
                                          size_t count) {
  // Skip if there is a batch update in progress.
  if (batch_update_)
    return;

  CookiesTreeModel* tree_model = static_cast<CookiesTreeModel*>(model);

  base::DictionaryValue args;
  if (parent == tree_model->GetRoot())
    args.Set(kId, std::make_unique<base::Value>());
  else
    args.SetString(kId, model_util_->GetTreeNodeId(tree_model->AsNode(parent)));
  args.SetInteger(kStart, int{start});
  args.SetInteger(kCount, int{count});
  FireWebUIListener("on-tree-item-removed", args);
}

void CookiesViewHandler::TreeModelBeginBatch(CookiesTreeModel* model) {
  DCHECK(!batch_update_);  // There should be no nested batch begin.
  batch_update_ = true;
}

void CookiesViewHandler::TreeModelEndBatch(CookiesTreeModel* model) {
  DCHECK(batch_update_);
  batch_update_ = false;

  if (request_.should_send_list) {
    SendLocalDataList(model->GetRoot());
  } else if (!request_.callback_id_.empty()) {
    ResolveJavascriptCallback(base::Value(request_.callback_id_),
                              (base::Value()));
    request_.Clear();
  }
}

void CookiesViewHandler::EnsureCookiesTreeModelCreated() {
  if (!cookies_tree_model_.get()) {
    Profile* profile = Profile::FromWebUI(web_ui());
    cookies_tree_model_ = CookiesTreeModel::CreateForProfile(profile);
    cookies_tree_model_->AddCookiesTreeObserver(this);
  }
}

void CookiesViewHandler::RecreateCookiesTreeModel() {
  cookies_tree_model_.reset();
  filter_.clear();
  sorted_sites_.clear();
  EnsureCookiesTreeModelCreated();

  CHECK(!request_.callback_id_.empty());
  ResolveJavascriptCallback(base::Value(request_.callback_id_),
                            (base::Value()));
  request_.Clear();
}

void CookiesViewHandler::HandleGetCookieDetails(const base::ListValue* args) {
  CHECK(request_.callback_id_.empty());
  CHECK_EQ(2U, args->GetList().size());
  request_.callback_id_ = args->GetList()[0].GetString();
  std::string site = args->GetList()[1].GetString();

  AllowJavascript();
  const CookieTreeNode* node = model_util_->GetTreeNodeFromTitle(
      cookies_tree_model_->GetRoot(), base::UTF8ToUTF16(site));

  if (!node) {
    RejectJavascriptCallback(base::Value(request_.callback_id_), base::Value());
    request_.Clear();
    return;
  }

  SendCookieDetails(node);
}

void CookiesViewHandler::HandleGetNumCookiesString(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetList().size());
  std::string callback_id;
  callback_id = args->GetList()[0].GetString();
  int num_cookies = args->GetList()[1].GetInt();

  AllowJavascript();
  const base::string16 string =
      num_cookies > 0 ? l10n_util::GetPluralStringFUTF16(
                            IDS_SETTINGS_SITE_SETTINGS_NUM_COOKIES, num_cookies)
                      : base::string16();

  ResolveJavascriptCallback(base::Value(callback_id), base::Value(string));
}

void CookiesViewHandler::HandleGetDisplayList(const base::ListValue* args) {
  CHECK(request_.callback_id_.empty());
  CHECK_EQ(2U, args->GetList().size());
  request_.callback_id_ = args->GetList()[0].GetString();
  base::string16 filter = base::UTF8ToUTF16(args->GetList()[1].GetString());

  AllowJavascript();
  request_.should_send_list = true;
  // Resetting the filter is a heavy operation, avoid unnecessary filtering.
  if (filter != filter_) {
    filter_ = filter;
    sorted_sites_.clear();
    cookies_tree_model_->UpdateSearchResults(filter_);
    return;
  }
  SendLocalDataList(cookies_tree_model_->GetRoot());
}

void CookiesViewHandler::HandleReloadCookies(const base::ListValue* args) {
  CHECK(request_.callback_id_.empty());
  CHECK_EQ(1U, args->GetList().size());
  request_.callback_id_ = args->GetList()[0].GetString();

  AllowJavascript();
  RecreateCookiesTreeModel();
}

void CookiesViewHandler::HandleRemoveAll(const base::ListValue* args) {
  CHECK(request_.callback_id_.empty());
  CHECK_EQ(1U, args->GetList().size());
  request_.callback_id_ = args->GetList()[0].GetString();

  AllowJavascript();
  cookies_tree_model_->DeleteAllStoredObjects();
  sorted_sites_.clear();
}

void CookiesViewHandler::HandleRemove(const base::ListValue* args) {
  std::string node_path = args->GetList()[0].GetString();

  AllowJavascript();
  const CookieTreeNode* node = model_util_->GetTreeNodeFromPath(
      cookies_tree_model_->GetRoot(), node_path);
  if (node) {
    cookies_tree_model_->DeleteCookieNode(const_cast<CookieTreeNode*>(node));
    sorted_sites_.clear();
  }
}

void CookiesViewHandler::HandleRemoveThirdParty(const base::ListValue* args) {
  CHECK(request_.callback_id_.empty());
  CHECK_EQ(1U, args->GetList().size());
  request_.callback_id_ = args->GetList()[0].GetString();

  AllowJavascript();
  Profile* profile = Profile::FromWebUI(web_ui());
  ClearSameSiteNoneData(
      base::BindOnce(&CookiesViewHandler::RecreateCookiesTreeModel,
                     callback_weak_ptr_factory_.GetWeakPtr()),
      profile,
      /* clear_storage */ true);
}

void CookiesViewHandler::HandleRemoveShownItems(const base::ListValue* args) {
  CHECK_EQ(0U, args->GetList().size());

  AllowJavascript();
  CookieTreeNode* parent = cookies_tree_model_->GetRoot();
  while (!parent->children().empty())
    cookies_tree_model_->DeleteCookieNode(parent->children().front().get());
}

void CookiesViewHandler::HandleRemoveItem(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetList().size());
  CHECK(request_.callback_id_.empty());
  base::string16 site = base::UTF8ToUTF16(args->GetList()[0].GetString());

  AllowJavascript();
  CookieTreeNode* parent = cookies_tree_model_->GetRoot();
  const auto i = std::find_if(
      parent->children().cbegin(), parent->children().cend(),
      [&site](const auto& node) { return node->GetTitle() == site; });
  if (i != parent->children().cend()) {
    cookies_tree_model_->DeleteCookieNode(i->get());
    sorted_sites_.clear();
  }
}

void CookiesViewHandler::SendLocalDataList(const CookieTreeNode* parent) {
  CHECK(cookies_tree_model_.get());
  CHECK(request_.should_send_list);
  const size_t parent_child_count = parent->children().size();
  if (sorted_sites_.empty()) {
    // Sort the list by site.
    sorted_sites_.reserve(parent_child_count);  // Optimization, hint size.
    for (size_t i = 0; i < parent_child_count; ++i) {
      const base::string16& title = parent->children()[i]->GetTitle();
      sorted_sites_.push_back(LabelAndIndex(title, i));
    }
    std::sort(sorted_sites_.begin(), sorted_sites_.end());
  }

  // The layers in the CookieTree are:
  //   root - Top level.
  //   site - www.google.com, example.com, etc.
  //   category - Cookies, Local Storage, etc.
  //   item - Info on the actual thing.
  // Gather list of sites with some highlights of the categories and items.
  std::unique_ptr<base::ListValue> site_list(new base::ListValue);
  for (const auto& sorted_site : sorted_sites_) {
    const CookieTreeNode* site = parent->children()[sorted_site.second].get();
    base::string16 description;
    for (const auto& category : site->children()) {
      if (!description.empty())
        description += base::ASCIIToUTF16(", ");
      const auto node_type = category->GetDetailedInfo().node_type;
      size_t item_count = category->children().size();
      switch (node_type) {
        case CookieTreeNode::DetailedInfo::TYPE_QUOTA:
          // TODO(crbug.com/642955): Omit quota values until bug is addressed.
          continue;
        case CookieTreeNode::DetailedInfo::TYPE_COOKIE:
          DCHECK_EQ(0u, item_count);
          item_count = 1;
          FALLTHROUGH;
        case CookieTreeNode::DetailedInfo::TYPE_COOKIES:
          description += l10n_util::GetPluralStringFUTF16(
              IDS_SETTINGS_SITE_SETTINGS_NUM_COOKIES, int{item_count});
          break;
        default:
          int ids_value = GetCategoryLabelID(node_type);
          if (!ids_value) {
            // If we don't have a label to call it by, don't show it. Please add
            // a label ID if an expected category is not appearing in the UI.
            continue;
          }
          description += l10n_util::GetStringUTF16(ids_value);
          break;
      }
    }
    std::unique_ptr<base::DictionaryValue> list_info(new base::DictionaryValue);
    list_info->Set(kLocalData, std::make_unique<base::Value>(description));
    std::string title = base::UTF16ToUTF8(site->GetTitle());
    list_info->Set(kSite, std::make_unique<base::Value>(title));
    site_list->Append(std::move(list_info));
  }

  base::DictionaryValue response;
  response.Set(kItems, std::move(site_list));
  response.Set(kTotal,
               std::make_unique<base::Value>(int{sorted_sites_.size()}));

  ResolveJavascriptCallback(base::Value(request_.callback_id_), response);
  request_.Clear();
}

void CookiesViewHandler::SendChildren(const CookieTreeNode* parent) {
  std::unique_ptr<base::ListValue> children(new base::ListValue);
  // Passing false for |include_quota_nodes| since they don't reflect reality
  // until bug http://crbug.com/642955 is fixed and local/session storage is
  // counted against the total.
  model_util_->GetChildNodeList(parent, /*start=*/0, parent->children().size(),
                                /*include_quota_nodes=*/false, children.get());

  base::DictionaryValue args;
  if (parent == cookies_tree_model_->GetRoot())
    args.Set(kId, std::make_unique<base::Value>());
  else
    args.SetString(kId, model_util_->GetTreeNodeId(parent));
  args.Set(kChildren, std::move(children));

  ResolveJavascriptCallback(base::Value(request_.callback_id_), args);
  request_.Clear();
}

void CookiesViewHandler::SendCookieDetails(const CookieTreeNode* parent) {
  std::unique_ptr<base::ListValue> children(new base::ListValue);
  // Passing false for |include_quota_nodes| since they don't reflect reality
  // until bug http://crbug.com/642955 is fixed and local/session storage is
  // counted against the total.
  model_util_->GetChildNodeDetails(parent, false, children.get());

  base::DictionaryValue args;
  if (parent == cookies_tree_model_->GetRoot())
    args.Set(kId, std::make_unique<base::Value>());
  else
    args.SetString(kId, model_util_->GetTreeNodeId(parent));
  args.Set(kChildren, std::move(children));

  ResolveJavascriptCallback(base::Value(request_.callback_id_), args);
  request_.Clear();
}

}  // namespace settings
