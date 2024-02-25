// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/noop_site_data_writer.h"

#include "base/no_destructor.h"

namespace performance_manager {

NoopSiteDataWriter::~NoopSiteDataWriter() = default;

void NoopSiteDataWriter::NotifySiteLoaded(TabVisibility visibility) {}

void NoopSiteDataWriter::NotifySiteUnloaded(TabVisibility visibility) {}

void NoopSiteDataWriter::NotifySiteForegrounded(bool is_loaded) {}

void NoopSiteDataWriter::NotifySiteBackgrounded(bool is_loaded) {}

void NoopSiteDataWriter::NotifyUpdatesFaviconInBackground() {}

void NoopSiteDataWriter::NotifyUpdatesTitleInBackground() {}

void NoopSiteDataWriter::NotifyUsesAudioInBackground() {}

void NoopSiteDataWriter::NotifyLoadTimePerformanceMeasurement(
    base::TimeDelta load_duration,
    base::TimeDelta cpu_usage_estimate,
    uint64_t private_footprint_kb_estimate) {}

const url::Origin& NoopSiteDataWriter::Origin() const {
  static const base::NoDestructor<url::Origin> dummy_origin;
  return *dummy_origin;
}

NoopSiteDataWriter::NoopSiteDataWriter() : SiteDataWriter(nullptr) {}

}  // namespace performance_manager
