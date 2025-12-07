// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_change_registrar.h"

#include <ostream>

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

PrefChangeRegistrar::~PrefChangeRegistrar() {
  // If you see an invalid memory access in this destructor, this
  // PrefChangeRegistrar might be subscribed to an OffTheRecordProfileImpl that
  // has been destroyed. This should not happen any more but be warned.
  // Feel free to contact battre@chromium.org in case this happens.
  //
  // This can also happen for non-OTR profiles, when the
  // DestroyProfileOnBrowserClose flag is enabled. In that case, contact
  // nicolaso@chromium.org.
  RemoveAll();
}

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
  if (!service_) {
    NOTREACHED();
  }
  DCHECK(!IsObserved(path))
      << "Already had pref, \"" << path << "\", registered.";

  service_->AddPrefObserver(path, this);
  observers_.insert_or_assign(std::string(path), std::move(obs));
}

void PrefChangeRegistrar::AddMultiple(
    const std::initializer_list<std::string_view>& paths,
    base::RepeatingClosure obs) {
  for (std::string_view path : paths) {
    Add(path, obs);
  }
}

void PrefChangeRegistrar::Remove(std::string_view path) {
  DCHECK(IsObserved(path));

  // Use std::map::erase directly once C++23 is supported.
  auto it = observers_.find(path);
  observers_.erase(it);
  service_->RemovePrefObserver(path, this);
}

void PrefChangeRegistrar::RemoveAll() {
  for (const auto& [key, _] : observers_) {
    service_->RemovePrefObserver(key, this);
  }

  observers_.clear();
}

bool PrefChangeRegistrar::IsEmpty() const {
  return observers_.empty();
}

bool PrefChangeRegistrar::IsObserved(std::string_view pref) {
  return observers_.find(pref) != observers_.end();
}

void PrefChangeRegistrar::OnServiceDestroyed(PrefService* service) {
  Reset();
}

void PrefChangeRegistrar::OnPreferenceChanged(PrefService* service,
                                              std::string_view pref) {
  if (auto iter = observers_.find(pref); iter != observers_.end()) {
    iter->second.Run(pref);
  }
}

PrefService* PrefChangeRegistrar::prefs() {
  return service_;
}

const PrefService* PrefChangeRegistrar::prefs() const {
  return service_;
}
