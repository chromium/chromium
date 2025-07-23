// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_change_registrar.h"

#include <ostream>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"

namespace {

// Returns a copy of `view`.
std::string CopyStringView(std::string_view view) {
  return std::string(view);
}

}  // namespace

PrefChangeRegistrar::PrefChangeRegistrar() : service_(nullptr) {}

PrefChangeRegistrar::~PrefChangeRegistrar() = default;

void PrefChangeRegistrar::Init(PrefService* service) {
  DCHECK(IsEmpty() || service_ == service);
  service_ = service;
}

void PrefChangeRegistrar::Reset() {
  RemoveAll();
  service_ = nullptr;
}

void PrefChangeRegistrar::Add(std::string_view path,
                              base::RepeatingClosure obs) {
  Add(path, base::IgnoreArgs<std::string_view>(std::move(obs)));
}

void PrefChangeRegistrar::Add(std::string_view path, NamedChangeCallback obs) {
  Add(path, base::BindRepeating(&CopyStringView).Then(std::move(obs)));
}

void PrefChangeRegistrar::Add(std::string_view path,
                              NamedChangeAsViewCallback obs) {
  CHECK(service_);
  DCHECK(!IsObserved(path)) << "Already had pref, \"" << path
                            << "\", registered.";

  subscriptions_.insert(std::make_pair(
      path, service_->AddPrefChangedCallback(
                path, base::IgnoreArgs<PrefService*>(std::move(obs)))));
}

void PrefChangeRegistrar::Remove(std::string_view path) {
  DCHECK(IsObserved(path));

  // Use std::map::erase directly once C++23 is supported.
  auto it = subscriptions_.find(path);
  subscriptions_.erase(it);
}

void PrefChangeRegistrar::RemoveAll() {
  subscriptions_.clear();
}

bool PrefChangeRegistrar::IsEmpty() const {
  return subscriptions_.empty();
}

bool PrefChangeRegistrar::IsObserved(std::string_view pref) {
  return subscriptions_.find(pref) != subscriptions_.end();
}

PrefService* PrefChangeRegistrar::prefs() {
  return service_;
}

const PrefService* PrefChangeRegistrar::prefs() const {
  return service_;
}
