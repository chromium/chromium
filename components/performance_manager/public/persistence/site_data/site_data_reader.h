// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERSISTENCE_SITE_DATA_SITE_DATA_READER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERSISTENCE_SITE_DATA_SITE_DATA_READER_H_

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/performance_manager/public/persistence/site_data/feature_usage.h"

namespace performance_manager {

FORWARD_DECLARE_TEST(SiteDataReaderTest,
                     FreeingReaderDoesntCauseWriteOperation);

namespace internal {
class SiteDataImpl;
}  // namespace internal

class SiteDataReader {
 public:
  SiteDataReader();
  virtual ~SiteDataReader();

  // Accessors for the site characteristics usage.
  virtual SiteFeatureUsage UpdatesFaviconInBackground() const = 0;
  virtual SiteFeatureUsage UpdatesTitleInBackground() const = 0;
  virtual SiteFeatureUsage UsesAudioInBackground() const = 0;

  // Returns true if this reader is fully initialized and serving the most
  // authoritative data. This can initially return false as the backing store is
  // loaded asynchronously.
  virtual bool DataLoaded() const = 0;

  // Registers a callback that will be invoked when the data backing this object
  // has been loaded. Note that if "DataLoaded" is true at the time this is
  // called it may immediately invoke the callback. The callback will not be
  // invoked after this object has been destroyed.
  virtual void RegisterDataLoadedCallback(base::OnceClosure&& callback) = 0;
};

class SiteDataReaderImpl : public SiteDataReader {
 public:
  SiteDataReaderImpl(const SiteDataReaderImpl&) = delete;
  SiteDataReaderImpl& operator=(const SiteDataReaderImpl&) = delete;

  ~SiteDataReaderImpl() override;

  // SiteDataReader:
  SiteFeatureUsage UpdatesFaviconInBackground() const override;
  SiteFeatureUsage UpdatesTitleInBackground() const override;
  SiteFeatureUsage UsesAudioInBackground() const override;
  bool DataLoaded() const override;
  void RegisterDataLoadedCallback(base::OnceClosure&& callback) override;

 private:
  friend class SiteDataCacheImpl;
  friend class SiteDataReaderTest;

  FRIEND_TEST_ALL_PREFIXES(SiteDataReaderTest,
                           DestroyingReaderCancelsPendingCallbacks);
  FRIEND_TEST_ALL_PREFIXES(SiteDataReaderTest,
                           FreeingReaderDoesntCauseWriteOperation);
  FRIEND_TEST_ALL_PREFIXES(SiteDataReaderTest, OnDataLoadedCallbackInvoked);

  // Private constructor, these objects are meant to be created by a site data
  // store.
  explicit SiteDataReaderImpl(scoped_refptr<internal::SiteDataImpl> impl);

  // Runs the provided closure. This is used as a wrapper so that callbacks
  // registered with the |impl_| by this reader are invalidated when the
  // reader is destroyed.
  void RunClosure(base::OnceClosure&& closure);

  // The SiteDataImpl object we delegate to.
  const scoped_refptr<internal::SiteDataImpl> impl_;

  // Used for invalidating callbacks.
  base::WeakPtrFactory<SiteDataReaderImpl> weak_factory_{this};
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERSISTENCE_SITE_DATA_SITE_DATA_READER_H_
