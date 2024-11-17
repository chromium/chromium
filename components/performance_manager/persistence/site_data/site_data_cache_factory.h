// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACTORY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACTORY_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/performance_manager/persistence/site_data/site_data_cache.h"
#include "content/public/browser/browser_context.h"

namespace content {
class BrowserContext;
}

namespace performance_manager {

class SiteDataCacheInspector;

// This class is responsible for tracking the SiteDataCache instances associated
// with each browser context. Instances of this class should be created and used
// from the same sequence (this is enforced via a sequence checker). It is the
// counterpart of the SiteDataCacheFacadeFactory living on the UI thread.
class SiteDataCacheFactory {
 public:
  SiteDataCacheFactory();

  SiteDataCacheFactory(const SiteDataCacheFactory&) = delete;
  SiteDataCacheFactory& operator=(const SiteDataCacheFactory&) = delete;

  ~SiteDataCacheFactory();

  // Returns a pointer to the global instance.
  static SiteDataCacheFactory* GetInstance();

  // Returns a pointer to the data cache associated with |browser_context_id|,
  // or null if there's no cache for this context yet.
  SiteDataCache* GetDataCacheForBrowserContext(
      const std::string& browser_context_id) const;

  // Returns the data cache inspector associated with |browser_context_id|, or
  // null if there's no data cache inspector for this context yet.
  SiteDataCacheInspector* GetInspectorForBrowserContext(
      const std::string& browser_context_id) const;

  // Sets the inspector instance associated with a given browser context.
  // If |inspector| is nullptr the association is cleared.
  // The caller must ensure that |inspector|'s registration is cleared before
  // |inspector| or |browser_context| are deleted.
  // The intent is for this to be called from the SiteDataCache implementation
  // class' constructors and destructors.
  void SetDataCacheInspectorForBrowserContext(
      SiteDataCacheInspector* inspector,
      const std::string& browser_context_id);

  // Testing functions to check if the data cache associated with
  // |browser_context_id| is recording.
  bool IsDataCacheRecordingForTesting(const std::string& browser_context_id);

  // Set the cache for a given browser context, this will replace any existing
  // cache.
  void SetCacheForTesting(const std::string& browser_context_id,
                          std::unique_ptr<SiteDataCache> cache);

  void SetCacheInspectorForTesting(const std::string& browser_context_id,
                                   SiteDataCacheInspector* inspector);

  // Implementation of the corresponding *OnUIThread public static functions
  // that runs on this object's task runner.
  void OnBrowserContextCreated(const std::string& browser_context_id,
                               const base::FilePath& context_path,
                               std::optional<std::string> parent_context_id);
  void OnBrowserContextDestroyed(const std::string& browser_context_id);

 private:
  // A map that associates a BrowserContext's ID with a SiteDataCache. This
  // object owns the caches.
  base::flat_map<std::string, std::unique_ptr<SiteDataCache>> data_cache_map_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // A map that associates a BrowserContext's ID with a SiteDataCacheInspector.
  base::flat_map<std::string, raw_ptr<SiteDataCacheInspector, CtnExperimental>>
      data_cache_inspector_map_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACTORY_H_
