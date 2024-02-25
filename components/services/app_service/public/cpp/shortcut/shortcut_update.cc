// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/shortcut/shortcut_update.h"
#include <type_traits>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/macros.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"

namespace apps {

ShortcutUpdate::ShortcutUpdate(const Shortcut* state, const Shortcut* delta)
    : state_(state), delta_(delta) {
  CHECK(state_ || delta_);
  if (state_ && delta_) {
    CHECK_EQ(state_->shortcut_id, delta->shortcut_id);
    CHECK_EQ(state_->host_app_id, delta->host_app_id);
    CHECK_EQ(state_->local_id, delta->local_id);
  }
}

bool ShortcutUpdate::operator==(const ShortcutUpdate& rhs) const {
  bool states_are_same = false;
  bool deltas_are_same = false;
  if (!this->state_ && !rhs.state_) {
    states_are_same = true;
  }
  if (this->state_ && rhs.state_) {
    states_are_same = *(this->state_) == *(rhs.state_);
  }

  if (!this->delta_ && !rhs.delta_) {
    deltas_are_same = true;
  }
  if (this->delta_ && rhs.delta_) {
    deltas_are_same = *(this->delta_) == *(rhs.delta_);
  }
  return states_are_same && deltas_are_same;
}

void ShortcutUpdate::Merge(Shortcut* state, const Shortcut* delta) {
  CHECK(state);
  if (!delta) {
    return;
  }

  if (delta->shortcut_id != state->shortcut_id) {
    LOG(ERROR) << "inconsistent shortcut_id: " << delta->shortcut_id << " vs "
               << state->shortcut_id;
    return;
  }

  if (delta->host_app_id != state->host_app_id) {
    LOG(ERROR) << "inconsistent host_app_id: " << delta->host_app_id << " vs "
               << state->host_app_id;
    return;
  }

  if (delta->local_id != state->local_id) {
    LOG(ERROR) << "inconsistent local_id: " << delta->local_id << " vs "
               << state->local_id;
    return;
  }

  SET_OPTIONAL_VALUE(name);
  SET_ENUM_VALUE(shortcut_source, ShortcutSource::kUnknown);

  state->icon_key = MergeIconKey(
      state && state->icon_key.has_value() ? &state->icon_key.value() : nullptr,
      delta && delta->icon_key.has_value() ? &delta->icon_key.value()
                                           : nullptr);

  SET_OPTIONAL_VALUE(allow_removal);
  // When adding new fields to the Shortcut struct, this function should also
  // be updated.
}

const ShortcutId& ShortcutUpdate::ShortcutId() const {
  return delta_ ? delta_->shortcut_id : state_->shortcut_id;
}

const std::string& ShortcutUpdate::HostAppId() const {
  return delta_ ? delta_->host_app_id : state_->host_app_id;
}

const std::string& ShortcutUpdate::LocalId() const {
  return delta_ ? delta_->local_id : state_->local_id;
}

const std::string& ShortcutUpdate::Name() const {
  GET_VALUE_WITH_FALLBACK(name, base::EmptyString())
}

bool ShortcutUpdate::NameChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(name);
}

ShortcutSource ShortcutUpdate::ShortcutSource() const {
  GET_VALUE_WITH_DEFAULT_VALUE(shortcut_source, ShortcutSource::kUnknown);
}

bool ShortcutUpdate::ShortcutSourceChanged() const {
  IS_VALUE_CHANGED_WITH_DEFAULT_VALUE(shortcut_source,
                                      ShortcutSource::kUnknown);
}

std::optional<apps::IconKey> ShortcutUpdate::IconKey() const {
  return MergeIconKey(
      state_ && state_->icon_key.has_value() ? &state_->icon_key.value()
                                             : nullptr,
      delta_ && delta_->icon_key.has_value() ? &delta_->icon_key.value()
                                             : nullptr);
}

bool ShortcutUpdate::IconKeyChanged() const {
  if (!delta_ || !delta_->icon_key.has_value()) {
    return false;
  }
  if (!state_ || !state_->icon_key.has_value()) {
    return true;
  }
  return MergeIconKey(&(state_->icon_key.value()),
                      &(delta_->icon_key.value())) != state_->icon_key;
}

std::optional<bool> ShortcutUpdate::AllowRemoval() const {
  GET_VALUE_WITH_FALLBACK(allow_removal, std::nullopt)
}

bool ShortcutUpdate::AllowRemovalChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(allow_removal);
}

bool ShortcutUpdate::ShortcutInitialized() const {
  return !state_ && delta_;
}

// For logging and debug purposes.
COMPONENT_EXPORT(SHORTCUT)
std::ostream& operator<<(std::ostream& out,
                         const ShortcutUpdate& shortcut_update) {
  out << "ShortcutId: " << shortcut_update.ShortcutId() << std::endl;
  out << "HostAppId: " << shortcut_update.HostAppId() << std::endl;
  out << "LocalId: " << shortcut_update.LocalId() << std::endl;
  out << "Name: " << shortcut_update.Name() << std::endl;
  out << "ShortcutSource: " << EnumToString(shortcut_update.ShortcutSource())
      << std::endl;
  out << "AllowRemoval: " << PRINT_OPTIONAL_BOOL(shortcut_update.AllowRemoval())
      << std::endl;
  return out;
}

}  // namespace apps
