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
#include "base/strings/string16.h"
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

 private:
  friend class CookiesViewHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(CookiesViewHandlerTest,
                           HandleReloadCookiesAndGetDisplayList);

  // Creates the CookiesTreeModel if necessary.
  void EnsureCookiesTreeModelCreated();

  // Resets the CookiesTreeModel, the current |filter_|, and the site list.
  void RecreateCookiesTreeModel();

  // Set |filter_| and get a portion (or all) of the list items.
  void HandleGetDisplayList(const base::ListValue* args);

  // Remove all items matching the current |filter_|.
  void HandleRemoveShownItems(const base::ListValue* args);

  // Remove a single item.
  void HandleRemoveItem(const base::ListValue* args);

  // Retrieve cookie details for a specific site.
  void HandleGetCookieDetails(const base::ListValue* args);

  // Gets a plural string for the given number of cookies.
  void HandleGetNumCookiesString(const base::ListValue* args);

  // Remove all sites data.
  void HandleRemoveAll(const base::ListValue* args);

  // Remove selected sites data.
  void HandleRemove(const base::ListValue* args);

  // Removes cookies and site data available in third-party contexts.
  void HandleRemoveThirdParty(const base::ListValue* args);

  // Get children nodes data and pass it to 'CookiesView.loadChildren' to
  // update the WebUI.
  void SendChildren(const CookieTreeNode* parent);

  void SendLocalDataList(const CookieTreeNode* parent);

  // Package and send cookie details for a site.
  void SendCookieDetails(const CookieTreeNode* parent);

  // Reloads the CookiesTreeModel and passes the nodes to
  // 'CookiesView.loadChildren' to update the WebUI.
  void HandleReloadCookies(const base::ListValue* args);

  // Flag to indicate whether there is a batch update in progress.
  bool batch_update_;

  // The Cookies Tree model
  std::unique_ptr<CookiesTreeModel> cookies_tree_model_;

  // Only show items that contain |filter|.
  base::string16 filter_;

  struct Request {
    Request();
    void Clear();

    // Whether the request expects a list response.
    bool should_send_list;
    // The callback ID for the current outstanding request.
    std::string callback_id_;
  };

  // The current client request.
  Request request_;

  // Sorted index list, by site. Indexes refer to |model->GetRoot()| children.
  typedef std::pair<base::string16, size_t> LabelAndIndex;
  std::vector<LabelAndIndex> sorted_sites_;

  std::unique_ptr<CookiesTreeModelUtil> model_util_;

  // Used to cancel callbacks when JavaScript becomes disallowed.
  base::WeakPtrFactory<CookiesViewHandler> callback_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CookiesViewHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_COOKIES_VIEW_HANDLER_H_
