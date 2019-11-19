// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/client_memory.h"

#include <algorithm>

#include "base/strings/string_util.h"

namespace autofill_assistant {

ClientMemory::ClientMemory() {}
ClientMemory::~ClientMemory() {}

const autofill::CreditCard* ClientMemory::selected_card() const {
  if (selected_card_.has_value())
    return selected_card_->get();
  return nullptr;
}

bool ClientMemory::has_selected_card() const {
  return selected_card() != nullptr;
}

const autofill::AutofillProfile* ClientMemory::selected_address(
    const std::string& name) const {
  auto it = selected_addresses_.find(name);
  if (it != selected_addresses_.end())
    return it->second.get();

  return nullptr;
}

void ClientMemory::set_selected_card(
    std::unique_ptr<autofill::CreditCard> card) {
  selected_card_ = std::move(card);
}

void ClientMemory::set_selected_address(
    const std::string& name,
    std::unique_ptr<autofill::AutofillProfile> address) {
  selected_addresses_[name] = std::move(address);
}

bool ClientMemory::has_selected_address(const std::string& name) const {
  return selected_address(name) != nullptr;
}

void ClientMemory::set_selected_login(const WebsiteLoginFetcher::Login& login) {
  selected_login_ = login;
}

bool ClientMemory::has_selected_login() const {
  return selected_login() != nullptr;
}

const WebsiteLoginFetcher::Login* ClientMemory::selected_login() const {
  if (selected_login_.has_value()) {
    return &(*selected_login_);
  }
  return nullptr;
}

const std::string* ClientMemory::additional_value(const std::string& key) {
  auto it = additional_values_.find(key);
  if (it == additional_values_.end()) {
    return nullptr;
  }
  return &it->second;
}

bool ClientMemory::has_additional_value(const std::string& key) {
  return additional_values_.find(key) != additional_values_.end();
}

void ClientMemory::set_additional_value(const std::string& key,
                                        const std::string& value) {
  additional_values_[key] = value;
}

std::string ClientMemory::GetAllAddressKeyNames() const {
  std::vector<std::string> entries;
  for (const auto& entry : selected_addresses_) {
    entries.emplace_back(entry.first);
  }
  std::sort(entries.begin(), entries.end());
  return base::JoinString(entries, ",");
}

}  // namespace autofill_assistant
