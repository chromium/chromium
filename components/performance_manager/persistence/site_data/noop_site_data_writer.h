// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_NOOP_SITE_DATA_WRITER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_NOOP_SITE_DATA_WRITER_H_

#include "components/performance_manager/persistence/site_data/site_data_writer.h"
#include "url/origin.h"

namespace performance_manager {

// Specialization of a SiteDataWriter that doesn't record anything.
class NoopSiteDataWriter : public SiteDataWriter {
 public:
  NoopSiteDataWriter(const NoopSiteDataWriter&) = delete;
  NoopSiteDataWriter& operator=(const NoopSiteDataWriter&) = delete;

  ~NoopSiteDataWriter() override;

  // Implementation of SiteDataWriter:
  void NotifySiteLoaded(TabVisibility visibility) override;
  void NotifySiteUnloaded(TabVisibility visibility) override;
  void NotifySiteForegrounded(bool is_loaded) override;
  void NotifySiteBackgrounded(bool is_loaded) override;
  void NotifyUpdatesFaviconInBackground() override;
  void NotifyUpdatesTitleInBackground() override;
  void NotifyUsesAudioInBackground() override;
  void NotifyLoadTimePerformanceMeasurement(
      base::TimeDelta load_duration,
      base::TimeDelta cpu_usage_estimate,
      uint64_t private_footprint_kb_estimate) override;
  const url::Origin& Origin() const override;

 private:
  friend class NonRecordingSiteDataCache;
  // Private constructor, these objects are meant to be created by a
  // NonRecordingSiteDataCache.
  NoopSiteDataWriter();
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_NOOP_SITE_DATA_WRITER_H_
