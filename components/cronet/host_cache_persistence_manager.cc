// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/host_cache_persistence_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "net/log/net_log.h"

namespace cronet {

HostCachePersistenceManager::HostCachePersistenceManager(
    net::HostCache* cache,
    PrefService* pref_service,
    std::string pref_name,
    base::TimeDelta delay,
    net::NetLog* net_log)
    : cache_(cache),
      pref_service_(pref_service),
      pref_name_(pref_name),
      writing_pref_(false),
      delay_(delay),
      net_log_(net::NetLogWithSource::Make(
          net_log,
          net::NetLogSourceType::HOST_CACHE_PERSISTENCE_MANAGER)) {
  DCHECK(cache_);
  DCHECK(pref_service_);

  // Get the initial value of the pref if it's already initialized.
  if (pref_service_->HasPrefPath(pref_name_))
    ReadFromDisk();

  registrar_.Init(pref_service_);
  registrar_.Add(pref_name_,
                 base::Bind(&HostCachePersistenceManager::ReadFromDisk,
                            weak_factory_.GetWeakPtr()));
  cache_->set_persistence_delegate(this);
}

HostCachePersistenceManager::~HostCachePersistenceManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timer_.Stop();
  registrar_.RemoveAll();
  cache_->set_persistence_delegate(nullptr);
}

void HostCachePersistenceManager::ReadFromDisk() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (writing_pref_)
    return;

  net_log_.BeginEvent(net::NetLogEventType::HOST_CACHE_PREF_READ);
  const base::ListValue* pref_value = pref_service_->GetList(pref_name_);
  bool success = cache_->RestoreFromListValue(*pref_value);
  net_log_.AddEntryWithBoolParams(net::NetLogEventType::HOST_CACHE_PREF_READ,
                                  net::NetLogEventPhase::END, "success",
                                  success);

  UMA_HISTOGRAM_BOOLEAN("DNS.HostCache.RestoreSuccess", success);
  UMA_HISTOGRAM_COUNTS_1000("DNS.HostCache.RestoreSize", pref_value->GetSize());
}

void HostCachePersistenceManager::ScheduleWrite() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (timer_.IsRunning())
    return;

  net_log_.AddEvent(net::NetLogEventType::HOST_CACHE_PERSISTENCE_START_TIMER);
  timer_.Start(FROM_HERE, delay_,
               base::BindOnce(&HostCachePersistenceManager::WriteToDisk,
                              weak_factory_.GetWeakPtr()));
}

void HostCachePersistenceManager::WriteToDisk() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  net_log_.AddEvent(net::NetLogEventType::HOST_CACHE_PREF_WRITE);
  base::ListValue value;
  cache_->GetAsListValue(&value, false);
  writing_pref_ = true;
  pref_service_->Set(pref_name_, value);
  writing_pref_ = false;
}

}  // namespace cronet
