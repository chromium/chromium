// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/stored_credential.h"

#include <algorithm>
#include <ranges>

namespace password_manager {

StoredCredential::StoredCredential() = default;

StoredCredential::StoredCredential(StoredCredential&&) = default;

StoredCredential& StoredCredential::operator=(StoredCredential&&) = default;

StoredCredential::~StoredCredential() = default;

bool AreStoredCredentialUniqueKeysEqual(const StoredCredential& left,
                                        const StoredCredential& right) {
  return StoredCredentialUniqueKey(left) == StoredCredentialUniqueKey(right);
}

std::optional<std::u16string> StoredCredential::GetPasswordBackup() const {
  for (const auto& note : notes) {
    if (note.unique_display_name ==
        PasswordNote::kPasswordChangeBackupNoteName) {
      return !note.value.empty() ? std::make_optional(note.value)
                                 : std::nullopt;
    }
  }
  return std::nullopt;
}

std::optional<base::Time> StoredCredential::GetPasswordBackupDateCreated()
    const {
  for (const auto& note : notes) {
    if (note.unique_display_name ==
        PasswordNote::kPasswordChangeBackupNoteName) {
      return !note.value.empty() ? std::make_optional(note.date_created)
                                 : std::nullopt;
    }
  }
  return std::nullopt;
}

std::u16string StoredCredential::GetPasswordNote() const {
  auto it = std::ranges::find(notes, std::u16string(),
                              &PasswordNote::unique_display_name);
  return it != notes.end() ? it->value : std::u16string();
}

void StoredCredential::SetPasswordNote(const std::u16string& new_note_value) {
  auto it = std::ranges::find(notes, std::u16string(),
                              &PasswordNote::unique_display_name);
  if (it == notes.end()) {
    notes.emplace_back(std::u16string(), new_note_value, base::Time::Now(),
                       /*hide_by_default=*/false);
    return;
  }

  if (it->value.empty()) {
    it->date_created = base::Time::Now();
  }

  it->value = new_note_value;
}

void StoredCredential::DeletePasswordBackupNote() {
  std::erase_if(notes, [](const PasswordNote& note) {
    return note.unique_display_name ==
           PasswordNote::kPasswordChangeBackupNoteName;
  });
}

}  // namespace password_manager
