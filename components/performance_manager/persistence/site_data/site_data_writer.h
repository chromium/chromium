// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_WRITER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_WRITER_H_

#include "base/memory/ref_counted.h"
#include "components/performance_manager/persistence/site_data/site_data_impl.h"

namespace performance_manager {

// This writer is initially in an unloaded state, a |NotifySiteLoaded| event
// should be sent if/when the tab using it gets loaded.
class SiteDataWriter {
 public:
  SiteDataWriter(const SiteDataWriter&) = delete;
  SiteDataWriter& operator=(const SiteDataWriter&) = delete;

  virtual ~SiteDataWriter();

  // Records tab load/unload events.
  virtual void NotifySiteLoaded(TabVisibility visibility);
  virtual void NotifySiteUnloaded(TabVisibility visibility);

  // Records visibility change events.
  virtual void NotifySiteForegrounded(bool is_loaded);
  virtual void NotifySiteBackgrounded(bool is_loaded);

  // Records feature usage.
  virtual void NotifyUpdatesFaviconInBackground();
  virtual void NotifyUpdatesTitleInBackground();
  virtual void NotifyUsesAudioInBackground();

  // Records performance measurements.
  virtual void NotifyLoadTimePerformanceMeasurement(
      base::TimeDelta load_duration,
      base::TimeDelta cpu_usage_estimate,
      uint64_t private_footprint_kb_estimate);

  virtual const url::Origin& Origin() const;

  internal::SiteDataImpl* impl_for_testing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return impl_.get();
  }

 protected:
  friend class SiteDataWriterTest;
  friend class SiteDataCacheImpl;
  friend class LenientMockDataWriter;

  // Protected constructor, these objects are meant to be created by a site data
  // store.
  explicit SiteDataWriter(scoped_refptr<internal::SiteDataImpl> impl);

 private:
  // The SiteDataImpl object we delegate to.
  const scoped_refptr<internal::SiteDataImpl> impl_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_WRITER_H_
