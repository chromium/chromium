// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_search_item.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "ui/actions/actions.h"

ActionAppMenuSearchItem::Builder::Builder() = default;
ActionAppMenuSearchItem::Builder::Builder(Builder&&) noexcept = default;
ActionAppMenuSearchItem::Builder& ActionAppMenuSearchItem::Builder::operator=(
    Builder&&) noexcept = default;
ActionAppMenuSearchItem::Builder::~Builder() = default;

ActionAppMenuSearchItem::Builder& ActionAppMenuSearchItem::Builder::SetType(
    Type type) {
  type_ = type;
  return *this;
}

ActionAppMenuSearchItem::Builder& ActionAppMenuSearchItem::Builder::SetAction(
    actions::BaseAction* action) {
  action_ = action;
  return *this;
}

ActionAppMenuSearchItem::Builder& ActionAppMenuSearchItem::Builder::SetTitle(
    std::u16string title) {
  title_ = std::move(title);
  return *this;
}

ActionAppMenuSearchItem::Builder&
ActionAppMenuSearchItem::Builder::SetSecondaryText(
    std::u16string secondary_text) {
  secondary_text_ = std::move(secondary_text);
  return *this;
}

ActionAppMenuSearchItem::Builder& ActionAppMenuSearchItem::Builder::SetSynonyms(
    std::vector<std::u16string> synonyms) {
  synonyms_ = std::move(synonyms);
  return *this;
}

std::unique_ptr<ActionAppMenuSearchItem>
ActionAppMenuSearchItem::Builder::Build() {
  CHECK(type_.has_value());
  CHECK(action_);
  CHECK(title_.has_value());

  actions::ActionItem* action_item = action_->GetActionItem();
  CHECK(action_item);

  auto search_item = std::make_unique<ActionAppMenuSearchItem>();
  search_item->SetType(*type_);
  search_item->SetActionItem(action_item);
  search_item->SetTitle(std::move(*title_));
  search_item->SetSecondaryText(std::move(secondary_text_));
  search_item->SetSynonyms(std::move(synonyms_));
  return search_item;
}

ActionAppMenuSearchItem::ActionAppMenuSearchItem() = default;

ActionAppMenuSearchItem::~ActionAppMenuSearchItem() = default;

void ActionAppMenuSearchItem::SetActionItem(actions::ActionItem* action_item) {
  action_item_ = action_item ? action_item->GetAsWeakPtr() : nullptr;
}

const std::u16string& ActionAppMenuSearchItem::GetTitle() const {
  return title_;
}

const std::u16string& ActionAppMenuSearchItem::GetSecondaryText() const {
  return secondary_text_;
}

const std::vector<std::u16string>& ActionAppMenuSearchItem::GetSynonyms()
    const {
  return synonyms_;
}
