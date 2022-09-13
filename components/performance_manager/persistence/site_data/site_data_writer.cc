// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/site_data_writer.h"

#include <utility>

namespace performance_manager {

SiteDataWriter::~SiteDataWriter() = default;

void SiteDataWriter::NotifySiteLoaded(TabVisibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  impl_->NotifySiteLoaded();

  if (visibility == TabVisibility::kBackground)
    impl_->NotifyLoadedSiteBackgrounded();
}

void SiteDataWriter::NotifySiteUnloaded(TabVisibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  impl_->NotifySiteUnloaded(visibility);
}

void SiteDataWriter::NotifySiteForegrounded(bool is_loaded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_loaded)
    impl_->NotifyLoadedSiteForegrounded();
}

void SiteDataWriter::NotifySiteBackgrounded(bool is_loaded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_loaded)
    impl_->NotifyLoadedSiteBackgrounded();
}

void SiteDataWriter::NotifyUpdatesFaviconInBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  impl_->NotifyUpdatesFaviconInBackground();
}

void SiteDataWriter::NotifyUpdatesTitleInBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  impl_->NotifyUpdatesTitleInBackground();
}

void SiteDataWriter::NotifyUsesAudioInBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(sebmarchand): Do not advance the background audio observation time
  // when the WebContents has never played audio.
  impl_->NotifyUsesAudioInBackground();
}

void SiteDataWriter::NotifyLoadTimePerformanceMeasurement(
    base::TimeDelta load_duration,
    base::TimeDelta cpu_usage_estimate,
    uint64_t private_footprint_kb_estimate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  impl_->NotifyLoadTimePerformanceMeasurement(load_duration, cpu_usage_estimate,
                                              private_footprint_kb_estimate);
}

const url::Origin& SiteDataWriter::Origin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return impl_->origin();
}

SiteDataWriter::SiteDataWriter(scoped_refptr<internal::SiteDataImpl> impl)
    : impl_(std::move(impl)) {
}

}  // namespace performance_manager
