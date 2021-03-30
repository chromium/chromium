// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_COOKIES_VIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_COOKIES_VIEW_HANDLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class CookiesTreeModelUtil;

namespace settings {

class CookiesViewHandler : public SettingsPageUIHandler,
                           public CookiesTreeModel::Observer {
 public:
  CookiesViewHandler();
  ~CookiesViewHandler() override;

  // SettingsPageUIHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

  // CookiesTreeModel::Observer:
  void TreeNodesAdded(ui::TreeModel* model,
                      ui::TreeModelNode* parent,
                      size_t start,
                      size_t count) override;
  void TreeNodesRemoved(ui::TreeModel* model,
                        ui::TreeModelNode* parent,
                        size_t start,
                        size_t count) override;
  void TreeNodeChanged(ui::TreeModel* model, ui::TreeModelNode* node) override {
  }
  void TreeModelBeginBatch(CookiesTreeModel* model) override;
  void TreeModelEndBatch(CookiesTreeModel* model) override;

  // Sets the tree model that will be used when the handler creates a tree
  // model, instead of building from from the profile.
  void SetCookiesTreeModelForTesting(
      std::unique_ptr<CookiesTreeModel> cookies_tree_model);

 private:
  friend class CookiesViewHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(CookiesViewHandlerTest, HandleGetDisplayList);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewHandlerTest, HandleRemoveShownItems);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewHandlerTest, SingleRequestDuringBatch);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewHandlerTest, NoStarvation);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewHandlerTest, ImmediateTreeOperation);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewHandlerTest,
                           HandleReloadCookiesAndGetDisplayList);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewHandlerTest, HandleGetCookieDetails);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewHandlerTest, HandleRemoveAll);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewHandlerTest, HandleRemoveItem);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewHandlerTest, HandleRemoveSite);

  // Recreates the CookiesTreeModel and resets the current |filter_|.
  void RecreateCookiesTreeModel();

  // Set |filter_| and get a portion (or all) of the list items.
  void HandleGetDisplayList(const base::ListValue* args);
  void GetDisplayList(std::string callback_id, const std::u16string& filter);

  // Remove all items matching the current |filter_|.
  void HandleRemoveShownItems(const base::ListValue* args);
  void RemoveShownItems();

  // Remove selected sites data.
  void HandleRemoveSite(const base::ListValue* args);
  void RemoveSite(const std::u16string& site);

  // Retrieve cookie details for a specific site.
  void HandleGetCookieDetails(const base::ListValue* args);
  void GetCookieDetails(const std::string& callback_id,
                        const std::string& site);

  // Gets a plural string for the given number of cookies.
  void HandleGetNumCookiesString(const base::ListValue* args);

  // Remove all sites data.
  void HandleRemoveAll(const base::ListValue* args);
  void RemoveAll(const std::string& callback_id);

  // Remove a single item.
  void HandleRemoveItem(const base::ListValue* args);
  void RemoveItem(const std::string& path);

  // Removes cookies and site data available in third-party contexts.
  void HandleRemoveThirdParty(const base::ListValue* args);

  void ReturnLocalDataList(const std::string& callback_id);

  // Reloads the CookiesTreeModel and passes the nodes to
  // 'CookiesView.loadChildren' to update the WebUI.
  void HandleReloadCookies(const base::ListValue* args);

  // Flag to indicate whether there is a batch update in progress.
  bool batch_update_;

  // The Cookies Tree model
  std::unique_ptr<CookiesTreeModel> cookies_tree_model_;

  // Cookies tree model which can be set for testing and will be used instead
  // of creating one directly from the profile.
  std::unique_ptr<CookiesTreeModel> cookies_tree_model_for_testing_;

  // Only show items that contain |filter|.
  std::u16string filter_;

  struct Request {
    // Specifies the batch behavior of the tree model when this request is run
    // against it. Batch behavior must be constant across invocations, and
    // defines when tasks can be queued.
    enum TreeModelBatchBehavior {
      // The request will not cause a batch operation to be started. Tasks may
      // only be queued when the request is first processed.
      NO_BATCH,

      // The request will cause a batch to start and finish syncronously. Tasks
      // may only be queued when the request is first processed.
      SYNC_BATCH,

      // The request will cause an asynchronous batch update to be run. Both
      // batch end and begin may occur asynchronously. Tasks may be queued when
      // the request is first processed, and when the batch is finished.
      ASYNC_BATCH
    };

    // Creates a request with a task to be queued when the request is first
    // processed.
    Request(TreeModelBatchBehavior batch_behavior,
            base::OnceClosure initial_task);

    // Creates a request with both a task to be queued when processed, and a
    // task to be queued when the tree model batch finishes. This constructor
    // implies |batch_behavior| == ASYNC_BATCH.
    Request(base::OnceClosure initial_task, base::OnceClosure batch_end_task);

    ~Request();
    Request(const Request&) = delete;
    Request(Request&& other);
    Request& operator=(const Request&&) = delete;

    TreeModelBatchBehavior batch_behavior;

    // Task which is run when the request reaches the front of the queue.
    // Task must only interact with the tree model in a synchronous manner.
    base::OnceClosure initial_task;

    // Optional task which is queued to run when the tree model batch ends.
    // Only valid when |batch_behavior| == ASYNC_BATCH. Must only interact with
    // the tree model in a synchronous manner.
    base::OnceClosure batch_end_task;
  };

  // The current client requests.
  std::queue<Request> pending_requests_;
  bool request_in_progress_ = false;

  // Check the request queue and process the first request if approproiate.
  void ProcessPendingRequests();

  // Signal that the request at the head of the request queue is complete.
  void RequestComplete();

  // Sorted index list, by site. Indexes refer to |model->GetRoot()| children.
  typedef std::pair<std::u16string, size_t> LabelAndIndex;
  std::vector<LabelAndIndex> sorted_sites_;

  std::unique_ptr<CookiesTreeModelUtil> model_util_;

  // Used to cancel callbacks when JavaScript becomes disallowed.
  base::WeakPtrFactory<CookiesViewHandler> callback_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CookiesViewHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_COOKIES_VIEW_HANDLER_H_
