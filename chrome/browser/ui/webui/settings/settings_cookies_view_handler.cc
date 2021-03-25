// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_cookies_view_handler.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
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

constexpr char kLocalData[] = "localData";
constexpr char kSite[] = "site";

CookiesViewHandler::Request::Request(TreeModelBatchBehavior batch_behavior,
                                     base::OnceClosure initial_task)
    : batch_behavior(batch_behavior), initial_task(std::move(initial_task)) {
  if (batch_behavior == Request::ASYNC_BATCH)
    batch_end_task = base::DoNothing::Once();
}

CookiesViewHandler::Request::Request(base::OnceClosure initial_task,
                                     base::OnceClosure batch_end_task)
    : batch_behavior(Request::TreeModelBatchBehavior::ASYNC_BATCH),
      initial_task(std::move(initial_task)),
      batch_end_task(std::move(batch_end_task)) {}

CookiesViewHandler::Request::~Request() = default;

CookiesViewHandler::Request::Request(Request&& other) {
  initial_task = std::move(other.initial_task);
  batch_end_task = std::move(other.batch_end_task);
}

CookiesViewHandler::CookiesViewHandler()
    : batch_update_(false), model_util_(new CookiesTreeModelUtil) {}

CookiesViewHandler::~CookiesViewHandler() {
}

void CookiesViewHandler::OnJavascriptAllowed() {
  // Some requests assume that a tree model has already been created, creating
  // here ensures this is true.
  pending_requests_.emplace(
      Request::ASYNC_BATCH,
      base::BindOnce(&CookiesViewHandler::RecreateCookiesTreeModel,
                     callback_weak_ptr_factory_.GetWeakPtr()));
  ProcessPendingRequests();
}

void CookiesViewHandler::OnJavascriptDisallowed() {
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
  pending_requests_ = std::queue<Request>();
}

void CookiesViewHandler::RegisterMessages() {
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
      "localData.removeSite",
      base::BindRepeating(&CookiesViewHandler::HandleRemoveSite,
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
                                        size_t count) {}

void CookiesViewHandler::TreeNodesRemoved(ui::TreeModel* model,
                                          ui::TreeModelNode* parent,
                                          size_t start,
                                          size_t count) {
  // Skip if there is a batch update in progress.
  if (batch_update_)
    return;
  FireWebUIListener("on-tree-item-removed");
}

void CookiesViewHandler::TreeModelBeginBatch(CookiesTreeModel* model) {
  DCHECK(!batch_update_);  // There should be no nested batch begin.
  DCHECK(!pending_requests_.empty());
  batch_update_ = true;

  DCHECK_NE(Request::NO_BATCH, pending_requests_.front().batch_behavior);
}

void CookiesViewHandler::TreeModelEndBatch(CookiesTreeModel* model) {
  DCHECK(batch_update_);
  DCHECK(!pending_requests_.empty());
  batch_update_ = false;

  DCHECK_NE(Request::NO_BATCH, pending_requests_.front().batch_behavior);

  if (pending_requests_.front().batch_behavior == Request::ASYNC_BATCH) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(pending_requests_.front().batch_end_task));

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&CookiesViewHandler::RequestComplete,
                                  callback_weak_ptr_factory_.GetWeakPtr()));
  }
}

void CookiesViewHandler::SetCookiesTreeModelForTesting(
    std::unique_ptr<CookiesTreeModel> cookies_tree_model) {
  cookies_tree_model_for_testing_ = std::move(cookies_tree_model);
}

void CookiesViewHandler::RecreateCookiesTreeModel() {
  cookies_tree_model_.reset();
  filter_.clear();
  cookies_tree_model_ =
      cookies_tree_model_for_testing_.get()
          ? std::move(cookies_tree_model_for_testing_)
          : CookiesTreeModel::CreateForProfile(Profile::FromWebUI(web_ui()));
  cookies_tree_model_->AddCookiesTreeObserver(this);
}

void CookiesViewHandler::HandleGetCookieDetails(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetList().size());
  std::string callback_id = args->GetList()[0].GetString();
  std::string site = args->GetList()[1].GetString();

  AllowJavascript();
  pending_requests_.emplace(
      Request::NO_BATCH, base::BindOnce(&CookiesViewHandler::GetCookieDetails,
                                        callback_weak_ptr_factory_.GetWeakPtr(),
                                        callback_id, site));
  ProcessPendingRequests();
}

void CookiesViewHandler::GetCookieDetails(const std::string& callback_id,
                                          const std::string& site) {
  const CookieTreeNode* node = model_util_->GetTreeNodeFromTitle(
      cookies_tree_model_->GetRoot(), base::UTF8ToUTF16(site));

  if (!node) {
    RejectJavascriptCallback(base::Value(callback_id), base::Value());
    return;
  }

  base::ListValue children;
  // TODO (crbug.com/642955): Pass true for |include_quota_nodes| parameter
  // when quota nodes include local/session storage in the total.
  model_util_->GetChildNodeDetails(node, /* include_quota_nodes */ false,
                                   &children);

  ResolveJavascriptCallback(base::Value(callback_id), std::move(children));
}

void CookiesViewHandler::HandleGetNumCookiesString(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetList().size());
  std::string callback_id;
  callback_id = args->GetList()[0].GetString();
  int num_cookies = args->GetList()[1].GetInt();

  AllowJavascript();
  const std::u16string string =
      num_cookies > 0 ? l10n_util::GetPluralStringFUTF16(
                            IDS_SETTINGS_SITE_SETTINGS_NUM_COOKIES, num_cookies)
                      : std::u16string();

  ResolveJavascriptCallback(base::Value(callback_id), base::Value(string));
}

void CookiesViewHandler::HandleGetDisplayList(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetList().size());
  std::string callback_id = args->GetList()[0].GetString();
  std::u16string filter = base::UTF8ToUTF16(args->GetList()[1].GetString());

  AllowJavascript();
  pending_requests_.emplace(
      Request::SYNC_BATCH,
      base::BindOnce(&CookiesViewHandler::GetDisplayList,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id,
                     filter));

  ProcessPendingRequests();
}

void CookiesViewHandler::GetDisplayList(std::string callback_id,
                                        const std::u16string& filter) {
  if (filter != filter_) {
    filter_ = filter;
    cookies_tree_model_->UpdateSearchResults(filter_);
    DCHECK(!batch_update_) << "Expected CookiesTreeModel::UpdateSearchResults "
                           << "to execute synchronously.";
  }
  ReturnLocalDataList(callback_id);
}

void CookiesViewHandler::HandleReloadCookies(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetList().size());
  std::string callback_id = args->GetList()[0].GetString();

  // Allowing Javascript for the first time will queue a task to create a new
  // tree model. Thus the tree model only needs to be recreated if Javascript
  // has already been allowed. Reload cookies is often the first call made by
  // pages using this handler, so this avoids unnecessary work.
  if (IsJavascriptAllowed()) {
    pending_requests_.emplace(
        base::BindOnce(&CookiesViewHandler::RecreateCookiesTreeModel,
                       callback_weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CookiesViewHandler::ResolveJavascriptCallback,
                       callback_weak_ptr_factory_.GetWeakPtr(),
                       base::Value(callback_id), base::Value()));
  } else {
    AllowJavascript();
    pending_requests_.emplace(
        Request::NO_BATCH,
        base::BindOnce(&CookiesViewHandler::ResolveJavascriptCallback,
                       callback_weak_ptr_factory_.GetWeakPtr(),
                       base::Value(callback_id), base::Value()));
  }
  ProcessPendingRequests();
}

void CookiesViewHandler::HandleRemoveAll(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetList().size());
  AllowJavascript();

  std::string callback_id = args->GetList()[0].GetString();

  pending_requests_.emplace(
      Request::SYNC_BATCH,
      base::BindOnce(&CookiesViewHandler::RemoveAll,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id));
  ProcessPendingRequests();
}

void CookiesViewHandler::RemoveAll(const std::string& callback_id) {
  cookies_tree_model_->DeleteAllStoredObjects();
  ResolveJavascriptCallback(base::Value(callback_id), base::Value());
}

void CookiesViewHandler::HandleRemoveItem(const base::ListValue* args) {
  std::string node_path = args->GetList()[0].GetString();

  AllowJavascript();
  pending_requests_.emplace(
      Request::NO_BATCH,
      base::BindOnce(&CookiesViewHandler::RemoveItem,
                     callback_weak_ptr_factory_.GetWeakPtr(), node_path));
  ProcessPendingRequests();
}

void CookiesViewHandler::RemoveItem(const std::string& path) {
  const CookieTreeNode* node =
      model_util_->GetTreeNodeFromPath(cookies_tree_model_->GetRoot(), path);
  if (node) {
    cookies_tree_model_->DeleteCookieNode(const_cast<CookieTreeNode*>(node));
  }
}

void CookiesViewHandler::HandleRemoveThirdParty(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetList().size());
  std::string callback_id = args->GetList()[0].GetString();

  AllowJavascript();
  Profile* profile = Profile::FromWebUI(web_ui());

  pending_requests_.emplace(
      base::BindOnce(
          content::ClearSameSiteNoneData,
          base::BindOnce(&CookiesViewHandler::RecreateCookiesTreeModel,
                         callback_weak_ptr_factory_.GetWeakPtr()),
          profile),
      base::BindOnce(&CookiesViewHandler::ResolveJavascriptCallback,
                     callback_weak_ptr_factory_.GetWeakPtr(),
                     base::Value(callback_id), base::Value()));
  ProcessPendingRequests();
}

void CookiesViewHandler::HandleRemoveShownItems(const base::ListValue* args) {
  CHECK_EQ(0U, args->GetList().size());

  AllowJavascript();
  pending_requests_.emplace(
      Request::NO_BATCH,
      base::BindOnce(&CookiesViewHandler::RemoveShownItems,
                     callback_weak_ptr_factory_.GetWeakPtr()));
  ProcessPendingRequests();
}

void CookiesViewHandler::RemoveShownItems() {
  CookieTreeNode* parent = cookies_tree_model_->GetRoot();
  while (!parent->children().empty())
    cookies_tree_model_->DeleteCookieNode(parent->children().front().get());
}

void CookiesViewHandler::HandleRemoveSite(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetList().size());
  std::u16string site = base::UTF8ToUTF16(args->GetList()[0].GetString());
  AllowJavascript();
  pending_requests_.emplace(
      Request::NO_BATCH,
      base::BindOnce(&CookiesViewHandler::RemoveSite,
                     callback_weak_ptr_factory_.GetWeakPtr(), site));
  ProcessPendingRequests();
}

void CookiesViewHandler::RemoveSite(const std::u16string& site) {
  CookieTreeNode* parent = cookies_tree_model_->GetRoot();
  const auto i = std::find_if(
      parent->children().cbegin(), parent->children().cend(),
      [&site](const auto& node) { return node->GetTitle() == site; });
  if (i != parent->children().cend()) {
    cookies_tree_model_->DeleteCookieNode(i->get());
  }
}

void CookiesViewHandler::ReturnLocalDataList(const std::string& callback_id) {
  CHECK(cookies_tree_model_.get());
  auto* parent = cookies_tree_model_->GetRoot();

  // The layers in the CookieTree are:
  //   root - Top level.
  //   site - www.google.com, example.com, etc.
  //   category - Cookies, Local Storage, etc.
  //   item - Info on the actual thing.
  // Gather list of sites with some highlights of the categories and items.
  base::ListValue site_list;
  for (const auto& site : parent->children()) {
    std::u16string description;
    for (const auto& category : site->children()) {
      if (!description.empty())
        description += u", ";
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
    site_list.Append(std::move(list_info));
  }

  // Sort the list into alphabetical order based on site name.
  std::sort(site_list.begin(), site_list.end(),
            [=](const base::Value& a, const base::Value& b) {
              return *a.FindStringKey(kSite) < *b.FindStringKey(kSite);
            });

  ResolveJavascriptCallback(base::Value(callback_id), std::move(site_list));
}

void CookiesViewHandler::ProcessPendingRequests() {
  if (pending_requests_.empty())
    return;

  // To ensure that multiple requests do not run during a tree model batch
  // update, only tasks for a single request are queued at any one time.
  if (request_in_progress_)
    return;

  request_in_progress_ = true;

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, std::move(pending_requests_.front().initial_task));
  if (pending_requests_.front().batch_behavior != Request::ASYNC_BATCH) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&CookiesViewHandler::RequestComplete,
                                  callback_weak_ptr_factory_.GetWeakPtr()));
  }
}

void CookiesViewHandler::RequestComplete() {
  DCHECK(!pending_requests_.empty());
  DCHECK(!batch_update_);
  request_in_progress_ = false;
  pending_requests_.pop();
  ProcessPendingRequests();
}

}  // namespace settings
