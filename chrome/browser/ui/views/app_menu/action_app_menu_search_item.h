// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_SEARCH_ITEM_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_SEARCH_ITEM_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/fuzzy_search/fuzzy_search_item.h"

namespace actions {
class ActionItem;
class BaseAction;
}  // namespace actions

// Lightweight adapter class bridging an actions::BaseAction (or ActionItem) to
// the FuzzySearchItem interface used by FuzzyFinder.
class ActionAppMenuSearchItem : public FuzzySearchItem {
 public:
  enum class Type {
    kAction,
    kBookmark,
    kTabGroup,
    kRecentTabs,
  };

  class Builder final {
   public:
    Builder();
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;
    Builder(Builder&&) noexcept;
    Builder& operator=(Builder&&) noexcept;
    ~Builder();

    Builder& SetType(Type type);
    Builder& SetAction(actions::BaseAction* action);
    Builder& SetTitle(std::u16string title);
    Builder& SetSecondaryText(std::u16string secondary_text);
    Builder& SetSynonyms(std::vector<std::u16string> synonyms);

    [[nodiscard]] std::unique_ptr<ActionAppMenuSearchItem> Build();

   private:
    std::optional<Type> type_;
    raw_ptr<actions::BaseAction> action_ = nullptr;
    std::optional<std::u16string> title_;
    std::u16string secondary_text_;
    std::vector<std::u16string> synonyms_;
  };

  ActionAppMenuSearchItem();
  ActionAppMenuSearchItem(const ActionAppMenuSearchItem&) = delete;
  ActionAppMenuSearchItem& operator=(const ActionAppMenuSearchItem&) = delete;
  ~ActionAppMenuSearchItem() override;

  // FuzzySearchItem:
  const std::u16string& GetTitle() const override;
  const std::u16string& GetSecondaryText() const override;
  const std::vector<std::u16string>& GetSynonyms() const override;

  actions::ActionItem* GetActionItem() const { return action_item_.get(); }
  Type GetType() const { return type_; }

  void SetType(Type type) { type_ = type; }
  void SetActionItem(actions::ActionItem* action_item);
  void SetTitle(std::u16string title) { title_ = std::move(title); }
  void SetSecondaryText(std::u16string secondary_text) {
    secondary_text_ = std::move(secondary_text);
  }
  void SetSynonyms(std::vector<std::u16string> synonyms) {
    synonyms_ = std::move(synonyms);
  }

 private:
  Type type_;
  base::WeakPtr<actions::ActionItem> action_item_;
  std::u16string title_;
  std::u16string secondary_text_;
  std::vector<std::u16string> synonyms_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_SEARCH_ITEM_H_
