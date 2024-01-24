// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_change_registrar.h"

#include <ostream>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"

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

void PrefChangeRegistrar::Add(const std::string& path,
                              const base::RepeatingClosure& obs) {
  Add(path,
      base::BindRepeating(&PrefChangeRegistrar::InvokeUnnamedCallback, obs));
}

void PrefChangeRegistrar::Add(const std::string& path,
                              const NamedChangeCallback& obs) {
  if (!service_) {
    NOTREACHED();
    return;
  }
  DCHECK(!IsObserved(path)) << "Already had pref, \"" << path
                            << "\", registered.";

  service_->AddPrefObserver(path, this);
  observers_[path] = obs;
}

void PrefChangeRegistrar::Remove(const std::string& path) {
  DCHECK(IsObserved(path));

  observers_.erase(path);
  service_->RemovePrefObserver(path, this);
}

void PrefChangeRegistrar::RemoveAll() {
  for (ObserverMap::const_iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    service_->RemovePrefObserver(it->first, this);
  }

  observers_.clear();
}

bool PrefChangeRegistrar::IsEmpty() const {
  return observers_.empty();
}

bool PrefChangeRegistrar::IsObserved(const std::string& pref) {
  return observers_.find(pref) != observers_.end();
}

void PrefChangeRegistrar::OnPreferenceChanged(PrefService* service,
                                              const std::string& pref) {
  if (IsObserved(pref))
    observers_[pref].Run(pref);
}

void PrefChangeRegistrar::InvokeUnnamedCallback(
    const base::RepeatingClosure& callback,
    const std::string& pref_name) {
  callback.Run();
}

PrefService* PrefChangeRegistrar::prefs() {
  return service_;
}

const PrefService* PrefChangeRegistrar::prefs() const {
  return service_;
}
