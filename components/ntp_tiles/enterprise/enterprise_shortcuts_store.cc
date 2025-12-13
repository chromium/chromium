// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/enterprise/enterprise_shortcuts_store.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/ntp_tiles/custom_links_util.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/prefs/pref_service.h"

namespace ntp_tiles {

namespace {

// Parses an EnterpriseShortcut stored in prefs. Returns nullopt if the data is
// malformed.
std::optional<EnterpriseShortcut> EnterpriseShortcutFromDict(
    const base::Value::Dict& dict) {
  const std::string* url_string =
      dict.FindString(EnterpriseShortcutsStore::kDictionaryKeyUrl);
  const std::string* title_string =
      dict.FindString(EnterpriseShortcutsStore::kDictionaryKeyTitle);
  const std::optional<int> policy_origin_value =
      dict.FindInt(EnterpriseShortcutsStore::kDictionaryKeyPolicyOrigin);
  const std::optional<bool> hidden_by_user_value =
      dict.FindBool(EnterpriseShortcutsStore::kDictionaryKeyIsHiddenByUser);
  const std::optional<bool> allow_user_edit_value =
      dict.FindBool(EnterpriseShortcutsStore::kDictionaryKeyAllowUserEdit);
  const std::optional<bool> allow_user_delete_value =
      dict.FindBool(EnterpriseShortcutsStore::kDictionaryKeyAllowUserDelete);

  if (!url_string || !title_string) {
    return std::nullopt;
  }

  GURL url(*url_string);
  if (!url.is_valid()) {
    return std::nullopt;
  }

  // Assume false if this value was not stored.
  EnterpriseShortcut link;
  link.url = std::move(url);
  link.title = base::UTF8ToUTF16(*title_string);
  link.policy_origin = static_cast<EnterpriseShortcut::PolicyOrigin>(
      policy_origin_value.value_or(
          static_cast<int>(EnterpriseShortcut::PolicyOrigin::kNoPolicy)));
  link.is_hidden_by_user = hidden_by_user_value.value_or(false);
  link.allow_user_edit = allow_user_edit_value.value_or(false);
  link.allow_user_delete = allow_user_delete_value.value_or(false);
  return link;
}

}  // namespace

const char EnterpriseShortcutsStore::kDictionaryKeyUrl[] = "url";
const char EnterpriseShortcutsStore::kDictionaryKeyTitle[] = "title";
const char EnterpriseShortcutsStore::kDictionaryKeyPolicyOrigin[] =
    "policyOrigin";
const char EnterpriseShortcutsStore::kDictionaryKeyIsHiddenByUser[] =
    "isHiddenByUser";
const char EnterpriseShortcutsStore::kDictionaryKeyAllowUserEdit[] =
    "allowUserEdit";
const char EnterpriseShortcutsStore::kDictionaryKeyAllowUserDelete[] =
    "allowUserDelete";

EnterpriseShortcutsStore::EnterpriseShortcutsStore(PrefService* prefs)
    : prefs_(prefs) {
  DCHECK(prefs);

  base::RepeatingClosure callback =
      base::BindRepeating(&EnterpriseShortcutsStore::OnPreferenceChanged,
                          weak_ptr_factory_.GetWeakPtr());
  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(prefs::kEnterpriseShortcutsPolicyList, callback);
}

EnterpriseShortcutsStore::~EnterpriseShortcutsStore() = default;

std::vector<EnterpriseShortcut> EnterpriseShortcutsStore::RetrieveLinks() {
  std::vector<EnterpriseShortcut> user_links = RetrieveUserLinks();
  return !user_links.empty() ? user_links : RetrievePolicyLinks();
}

std::vector<EnterpriseShortcut> EnterpriseShortcutsStore::RetrieveUserLinks() {
  return RetrieveLinksFromPrefs(prefs::kEnterpriseShortcutsUserList);
}

std::vector<EnterpriseShortcut>
EnterpriseShortcutsStore::RetrievePolicyLinks() {
  return RetrieveLinksFromPrefs(prefs::kEnterpriseShortcutsPolicyList);
}

std::vector<EnterpriseShortcut>
EnterpriseShortcutsStore::RetrieveLinksFromPrefs(std::string_view pref_path) {
  std::vector<EnterpriseShortcut> links;
  const base::Value::List& stored_links = prefs_->GetList(pref_path);
  for (const base::Value& link : stored_links) {
    std::optional<EnterpriseShortcut> link_to_add =
        EnterpriseShortcutFromDict(link.GetDict());
    // `link_to_add` has no value if it contains an invalid link. Stop
    // processing links if so.
    if (!link_to_add.has_value()) {
      links.clear();
      return links;
    }
    links.emplace_back(link_to_add.value());
  }
  return links;
}

void EnterpriseShortcutsStore::StoreLinks(
    const std::vector<EnterpriseShortcut>& links) {
  base::Value::List new_link_list;
  for (const EnterpriseShortcut& link : links) {
    base::Value::Dict new_link;
    new_link.Set(kDictionaryKeyUrl, link.url.spec());
    new_link.Set(kDictionaryKeyTitle, link.title);
    new_link.Set(kDictionaryKeyPolicyOrigin,
                 static_cast<int>(link.policy_origin));
    new_link.Set(kDictionaryKeyIsHiddenByUser, link.is_hidden_by_user);
    new_link.Set(kDictionaryKeyAllowUserEdit, link.allow_user_edit);
    new_link.Set(kDictionaryKeyAllowUserDelete, link.allow_user_delete);
    new_link_list.Append(std::move(new_link));
  }
  prefs_->SetList(prefs::kEnterpriseShortcutsUserList,
                  std::move(new_link_list));
}

void EnterpriseShortcutsStore::ClearLinks() {
  prefs_->ClearPref(prefs::kEnterpriseShortcutsUserList);
}

void EnterpriseShortcutsStore::MergeLinkFromPolicy(
    std::vector<EnterpriseShortcut>& list_links,
    const EnterpriseShortcut& link_from_policy) {
  if (!link_from_policy.url.is_valid()) {
    return;
  }
  auto it = custom_links_util::FindLinkWithUrl<EnterpriseShortcut>(
      list_links, link_from_policy.url);
  if (it == list_links.end()) {
    // Current list does not already have link so add new link.
    list_links.push_back(link_from_policy);
  } else {
    // Current list already has link so update existing link.
    it->allow_user_edit = link_from_policy.allow_user_edit;
    // Force `is_hidden_by_user` back to false if `allow_user_delete` is false.
    it->allow_user_delete = link_from_policy.allow_user_delete;
    if (!it->allow_user_delete) {
      it->is_hidden_by_user = false;
    }
    // Do not update `title` if user has modified this link already.
    if (it->policy_origin != EnterpriseShortcut::PolicyOrigin::kNoPolicy) {
      it->title = link_from_policy.title;
    }
  }
}

void EnterpriseShortcutsStore::OnPreferenceChanged() {
  std::vector<EnterpriseShortcut> policy_links = RetrievePolicyLinks();
  std::vector<EnterpriseShortcut> list_user_links = RetrieveUserLinks();

  if (policy_links.empty()) {
    // If the policy is removed or empty, clear all user links.
    ClearLinks();
    closure_list_.Notify();
    return;
  }

  // If the user links list is empty, do nothing. `RetrieveLinks` will return
  // the policy links.
  if (list_user_links.empty()) {
    closure_list_.Notify();
    return;
  }

  // If there are user modifications, merge policy changes with user changes.
  for (const EnterpriseShortcut& link : policy_links) {
    MergeLinkFromPolicy(list_user_links, link);
  }

  std::erase_if(list_user_links, [&](const EnterpriseShortcut& link) {
    return std::ranges::find(policy_links, link.url,
                             &EnterpriseShortcut::url) == policy_links.end();
  });

  StoreLinks(list_user_links);
  closure_list_.Notify();
}

base::CallbackListSubscription
EnterpriseShortcutsStore::RegisterCallbackForOnChanged(
    base::RepeatingClosure callback) {
  return closure_list_.Add(callback);
}

// static
void EnterpriseShortcutsStore::RegisterProfilePrefs(
    PrefRegistrySimple* user_prefs) {
  user_prefs->RegisterListPref(prefs::kEnterpriseShortcutsUserList);
  user_prefs->RegisterListPref(prefs::kEnterpriseShortcutsPolicyList);
}

}  // namespace ntp_tiles
