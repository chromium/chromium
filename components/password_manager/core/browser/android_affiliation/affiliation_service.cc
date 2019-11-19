// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android_affiliation/affiliation_service.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_backend.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {

AffiliationService::AffiliationService(
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : backend_(nullptr), backend_task_runner_(backend_task_runner) {}

AffiliationService::~AffiliationService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (backend_) {
    backend_task_runner_->DeleteSoon(FROM_HERE, backend_);
    backend_ = nullptr;
  }
}

void AffiliationService::Initialize(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    const base::FilePath& db_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!backend_);
  backend_ = new AffiliationBackend(backend_task_runner_,
                                    base::DefaultClock::GetInstance(),
                                    base::DefaultTickClock::GetInstance());

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AffiliationBackend::Initialize,
                     base::Unretained(backend_), url_loader_factory->Clone(),
                     base::Unretained(network_connection_tracker), db_path));
}

void AffiliationService::GetAffiliationsAndBranding(
    const FacetURI& facet_uri,
    StrategyOnCacheMiss cache_miss_strategy,
    ResultCallback result_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AffiliationBackend::GetAffiliationsAndBranding,
                                base::Unretained(backend_), facet_uri,
                                cache_miss_strategy, std::move(result_callback),
                                base::SequencedTaskRunnerHandle::Get()));
}

void AffiliationService::Prefetch(const FacetURI& facet_uri,
                                  const base::Time& keep_fresh_until) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AffiliationBackend::Prefetch, base::Unretained(backend_),
                     facet_uri, keep_fresh_until));
}

void AffiliationService::CancelPrefetch(const FacetURI& facet_uri,
                                        const base::Time& keep_fresh_until) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AffiliationBackend::CancelPrefetch,
                     base::Unretained(backend_), facet_uri, keep_fresh_until));
}

void AffiliationService::TrimCacheForFacetURI(const FacetURI& facet_uri) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AffiliationBackend::TrimCacheForFacetURI,
                                base::Unretained(backend_), facet_uri));
}

}  // namespace password_manager
