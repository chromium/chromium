// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/performance_manager/persistence/site_data/site_data_impl.h"

namespace performance_manager {

SiteDataReader::SiteDataReader() = default;
SiteDataReader::~SiteDataReader() = default;

SiteDataReaderImpl::SiteDataReaderImpl(
    scoped_refptr<internal::SiteDataImpl> impl)
    : impl_(std::move(impl)) {}

SiteDataReaderImpl::~SiteDataReaderImpl() = default;

performance_manager::SiteFeatureUsage
SiteDataReaderImpl::UpdatesFaviconInBackground() const {
  return impl_->UpdatesFaviconInBackground();
}

performance_manager::SiteFeatureUsage
SiteDataReaderImpl::UpdatesTitleInBackground() const {
  return impl_->UpdatesTitleInBackground();
}

performance_manager::SiteFeatureUsage
SiteDataReaderImpl::UsesAudioInBackground() const {
  return impl_->UsesAudioInBackground();
}

bool SiteDataReaderImpl::DataLoaded() const {
  return impl_->DataLoaded();
}

void SiteDataReaderImpl::RegisterDataLoadedCallback(
    base::OnceClosure&& callback) {
  // Register a closure that is bound using a weak pointer to this instance.
  // In that way it won't be invoked by the underlying |impl_| after this
  // reader is destroyed.
  base::OnceClosure closure(base::BindOnce(&SiteDataReaderImpl::RunClosure,
                                           weak_factory_.GetWeakPtr(),
                                           std::move(callback)));
  impl_->RegisterDataLoadedCallback(std::move(closure));
}

void SiteDataReaderImpl::RunClosure(base::OnceClosure&& closure) {
  std::move(closure).Run();
}

}  // namespace performance_manager
