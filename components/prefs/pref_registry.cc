// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_registry.h"

#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/values.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/pref_store.h"

PrefRegistry::PrefRegistry()
    : defaults_(base::MakeRefCounted<DefaultPrefStore>()) {}

PrefRegistry::~PrefRegistry() {
}

uint32_t PrefRegistry::GetRegistrationFlags(std::string_view pref_name) const {
  const auto& it = registration_flags_.find(pref_name);
  return it != registration_flags_.end() ? it->second : NO_REGISTRATION_FLAGS;
}

scoped_refptr<PrefStore> PrefRegistry::defaults() {
  return defaults_.get();
}

PrefRegistry::const_iterator PrefRegistry::begin() const {
  return defaults_->begin();
}

PrefRegistry::const_iterator PrefRegistry::end() const {
  return defaults_->end();
}

void PrefRegistry::SetDefaultPrefValue(std::string_view pref_name,
                                       base::Value value) {
  const base::Value* current_value = nullptr;
  DCHECK(defaults_->GetValue(pref_name, &current_value))
      << "Setting default for unregistered pref: " << pref_name;
  DCHECK(value.type() == current_value->type())
      << "Wrong type for new default: " << pref_name;

  defaults_->ReplaceDefaultValue(pref_name, std::move(value));
}

void PrefRegistry::RegisterPreference(std::string_view path,
                                      base::Value default_value,
                                      uint32_t flags) {
  base::Value::Type orig_type = default_value.type();
  DCHECK(orig_type != base::Value::Type::NONE &&
         orig_type != base::Value::Type::BINARY) <<
         "invalid preference type: " << orig_type;
  DCHECK(!defaults_->GetValue(path, nullptr))
      << "Trying to register a previously registered pref: " << path;
  DCHECK(!base::Contains(registration_flags_, std::string(path)))
      << "Trying to register a previously registered pref: " << path;

  defaults_->SetDefaultValue(path, std::move(default_value));
  if (flags != NO_REGISTRATION_FLAGS) {
    registration_flags_.insert_or_assign(std::string(path), flags);
  }

  OnPrefRegistered(path, flags);
}

void PrefRegistry::OnPrefRegistered(std::string_view path, uint32_t flags) {}
