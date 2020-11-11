// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_service_factory.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/sequenced_task_runner.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_store.h"

PrefServiceFactory::PrefServiceFactory()
    : read_error_callback_(base::DoNothing()), async_(false) {}

PrefServiceFactory::~PrefServiceFactory() {}

void PrefServiceFactory::SetUserPrefsFile(
    const base::FilePath& prefs_file,
    base::SequencedTaskRunner* task_runner) {
  user_prefs_ =
      base::MakeRefCounted<JsonPrefStore>(prefs_file, nullptr, task_runner);
}

std::unique_ptr<PrefService> PrefServiceFactory::Create(
    scoped_refptr<PrefRegistry> pref_registry,
    std::unique_ptr<PrefValueStore::Delegate> delegate) {
  auto pref_notifier = std::make_unique<PrefNotifierImpl>();
  auto pref_value_store = std::make_unique<PrefValueStore>(
      managed_prefs_.get(), supervised_user_prefs_.get(),
      extension_prefs_.get(), command_line_prefs_.get(), user_prefs_.get(),
      recommended_prefs_.get(), pref_registry->defaults().get(),
      pref_notifier.get(), std::move(delegate));
  return std::make_unique<PrefService>(
      std::move(pref_notifier), std::move(pref_value_store), user_prefs_.get(),
      std::move(pref_registry), read_error_callback_, async_);
}

void PrefServiceFactory::ChangePrefValueStore(
    PrefService* pref_service,
    std::unique_ptr<PrefValueStore::Delegate> delegate) {
  pref_service->ChangePrefValueStore(
      managed_prefs_.get(), supervised_user_prefs_.get(),
      extension_prefs_.get(), recommended_prefs_.get(), std::move(delegate));
}
