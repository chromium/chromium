// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_COLLECTION_NODE_H_
#define CHROME_BROWSER_UI_TABS_TAB_COLLECTION_NODE_H_

#include <memory>
#include <variant>

#include "components/tabs/public/tab_collection_node_interface.h"

namespace tabs {

class TabCollection;
class TabInterface;

class TabCollectionNode : public TabCollectionNodeInterface {
 public:
  explicit TabCollectionNode(std::unique_ptr<TabInterface> tab_interface);
  explicit TabCollectionNode(std::unique_ptr<TabCollection> tab_collection);
  ~TabCollectionNode() override;

  TabCollectionNode(const TabCollectionNode&) = delete;
  void operator=(const TabCollectionNode&) = delete;

  Type GetType() const override;
  TabInterface* GetTabInterface() const override;
  TabCollection* GetTabCollection() const override;

 private:
  std::variant<std::unique_ptr<TabInterface>, std::unique_ptr<TabCollection>>
      holder_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_COLLECTION_NODE_H_
