// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/observable_authenticator_list.h"

#include <algorithm>
#include <utility>

#include "chrome/browser/webauthn/authenticator_list_observer.h"

ObservableAuthenticatorList::ObservableAuthenticatorList() = default;

ObservableAuthenticatorList::~ObservableAuthenticatorList() = default;

void ObservableAuthenticatorList::AddAuthenticator(
    AuthenticatorReference authenticator) {
  authenticator_list_.emplace_back(std::move(authenticator));
  if (observer_)
    observer_->OnAuthenticatorAdded(authenticator_list_.back());
}

void ObservableAuthenticatorList::RemoveAuthenticator(
    base::StringPiece authenticator_id) {
  auto it = GetAuthenticatorIterator(authenticator_id);
  if (it == authenticator_list_.end())
    return;

  auto removed_authenticator = std::move(*it);
  authenticator_list_.erase(it);

  if (observer_)
    observer_->OnAuthenticatorRemoved(removed_authenticator);
}

void ObservableAuthenticatorList::ChangeAuthenticatorId(
    base::StringPiece previous_id,
    std::string new_id) {
  auto* authenticator = GetAuthenticator(previous_id);
  if (!authenticator)
    return;

  if (observer_)
    observer_->OnAuthenticatorRemoved(*authenticator);

  authenticator->SetAuthenticatorId(std::move(new_id));
  if (observer_)
    observer_->OnAuthenticatorAdded(*authenticator);
}

void ObservableAuthenticatorList::ChangeAuthenticatorPairingMode(
    base::StringPiece authenticator_id,
    bool is_in_pairing_mode) {
  auto it = GetAuthenticatorIterator(authenticator_id);
  if (it == authenticator_list_.end())
    return;

  it->SetIsInPairingMode(is_in_pairing_mode);
  if (observer_)
    observer_->OnAuthenticatorPairingModeChanged(*it);
}

AuthenticatorReference* ObservableAuthenticatorList::GetAuthenticator(
    base::StringPiece authenticator_id) {
  auto it = GetAuthenticatorIterator(authenticator_id);
  if (it == authenticator_list_.end())
    return nullptr;

  return &*it;
}

void ObservableAuthenticatorList::SetObserver(
    AuthenticatorListObserver* observer) {
  DCHECK(!observer_);
  observer_ = observer;
}

void ObservableAuthenticatorList::RemoveObserver() {
  observer_ = nullptr;
}

ObservableAuthenticatorList::AuthenticatorListIterator
ObservableAuthenticatorList::GetAuthenticatorIterator(
    base::StringPiece authenticator_id) {
  return std::find_if(authenticator_list_.begin(), authenticator_list_.end(),
                      [authenticator_id](const auto& authenticator) {
                        return authenticator.authenticator_id() ==
                               authenticator_id;
                      });
}
