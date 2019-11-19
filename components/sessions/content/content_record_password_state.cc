// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_record_password_state.h"

#include "content/public/browser/navigation_entry.h"

namespace sessions {

namespace {
// The key used to store PasswordState in the NavigationEntry;
// We stash an enum value in the first character of the string16 that is
// associated with this key.
const char kPasswordStateKey[] = "sessions_password_state";

class PasswordStateData : public base::SupportsUserData::Data {
 public:
  explicit PasswordStateData(
      SerializedNavigationEntry::PasswordState password_state)
      : password_state_(password_state) {}
  ~PasswordStateData() override = default;

  SerializedNavigationEntry::PasswordState password_state() const {
    return password_state_;
  }

  // base::SupportsUserData::Data:
  std::unique_ptr<Data> Clone() override {
    return std::make_unique<PasswordStateData>(password_state_);
  }

 private:
  const SerializedNavigationEntry::PasswordState password_state_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStateData);
};

}  // namespace

SerializedNavigationEntry::PasswordState GetPasswordStateFromNavigation(
    content::NavigationEntry* entry) {
  PasswordStateData* data =
      static_cast<PasswordStateData*>(entry->GetUserData(kPasswordStateKey));
  return data ? data->password_state()
              : SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN;
}

void SetPasswordStateInNavigation(
    SerializedNavigationEntry::PasswordState state,
    content::NavigationEntry* entry) {
  entry->SetUserData(kPasswordStateKey,
                     std::make_unique<PasswordStateData>(state));
}

}  // namespace sessions
