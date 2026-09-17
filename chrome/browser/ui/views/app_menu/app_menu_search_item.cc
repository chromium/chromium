// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_search_item.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/no_destructor.h"
#include "ui/actions/actions.h"

AppMenuSearchItem::Builder::Builder() = default;
AppMenuSearchItem::Builder::Builder(Builder&&) noexcept = default;
AppMenuSearchItem::Builder& AppMenuSearchItem::Builder::operator=(
    Builder&&) noexcept = default;
AppMenuSearchItem::Builder::~Builder() = default;

AppMenuSearchItem::Builder& AppMenuSearchItem::Builder::SetType(Type type) {
  type_ = type;
  return *this;
}

AppMenuSearchItem::Builder& AppMenuSearchItem::Builder::SetAction(
    actions::BaseAction* action) {
  action_ = action;
  return *this;
}

AppMenuSearchItem::Builder& AppMenuSearchItem::Builder::SetTitle(
    std::u16string title) {
  title_ = std::move(title);
  return *this;
}

AppMenuSearchItem::Builder& AppMenuSearchItem::Builder::SetSecondaryText(
    std::u16string secondary_text) {
  secondary_text_ = std::move(secondary_text);
  return *this;
}

std::unique_ptr<AppMenuSearchItem> AppMenuSearchItem::Builder::Build() {
  CHECK(type_.has_value());
  CHECK(action_);
  CHECK(title_.has_value());

  actions::ActionItem* action_item = action_->GetActionItem();
  CHECK(action_item);

  auto search_item = std::make_unique<AppMenuSearchItem>();
  search_item->SetType(*type_);
  search_item->SetActionItem(action_item);
  search_item->SetTitle(std::move(*title_));
  search_item->SetSecondaryText(std::move(secondary_text_));
  return search_item;
}

AppMenuSearchItem::AppMenuSearchItem() = default;

AppMenuSearchItem::~AppMenuSearchItem() = default;

void AppMenuSearchItem::SetActionItem(actions::ActionItem* action_item) {
  action_item_ = action_item ? action_item->GetAsWeakPtr() : nullptr;
}

const std::u16string& AppMenuSearchItem::GetTitle() const {
  return title_;
}

const std::u16string& AppMenuSearchItem::GetSecondaryText() const {
  return secondary_text_;
}

const std::vector<std::u16string>& AppMenuSearchItem::GetSynonyms() const {
  static const base::NoDestructor<std::vector<std::u16string>> kEmptySynonyms;
  return action_item_ ? action_item_->GetSynonyms() : *kEmptySynonyms;
}
