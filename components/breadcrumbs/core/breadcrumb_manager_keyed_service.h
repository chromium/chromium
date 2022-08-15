// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_KEYED_SERVICE_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_KEYED_SERVICE_H_

#include <list>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace breadcrumbs {

class BreadcrumbManager;
class BreadcrumbManagerObserver;
class BreadcrumbPersistentStorageManager;

// Associates a BreadcrumbManager instance with a browser (BrowserState on iOS,
// BrowserContext on Desktop) - either incognito or normal.
class BreadcrumbManagerKeyedService : public KeyedService {
 public:
  explicit BreadcrumbManagerKeyedService(bool is_off_the_record);
  BreadcrumbManagerKeyedService(const BreadcrumbManagerKeyedService&) = delete;
  BreadcrumbManagerKeyedService& operator=(
      const BreadcrumbManagerKeyedService&) = delete;
  ~BreadcrumbManagerKeyedService() override;

  // Logs a breadcrumb |event| associated with the browser. Prepends the
  // |browsing_mode_| identifier to the event before passing it to the
  // |breadcrumb_manager_|.
  void AddEvent(const std::string& event);

  // Adds and removes observers to the underlying |breadcrumb_manager_|.
  void AddObserver(BreadcrumbManagerObserver* observer);
  void RemoveObserver(BreadcrumbManagerObserver* observer);

  // Returns up to |event_count_limit| events from the underlying
  // |breadcrumb_manager|. See |BreadcrumbManager::GetEvents| for returned event
  // details.
  const std::list<std::string> GetEvents(size_t event_count_limit) const;

  // Persists all events logged to |breadcrumb_manager_| to
  // |persistent_storage_manager|. If StartPersisting has already been called,
  // breadcrumbs will no longer be persisted to the previous
  // |persistent_storage_manager|.
  // NOTE: |persistent_storage_manager| must be non-null.
  void StartPersisting(
      BreadcrumbPersistentStorageManager* persistent_storage_manager);
  // Stops persisting events to |persistent_storage_manager_|. No-op if
  // |persistent_storage_manager_| is not set.
  void StopPersisting();
  // Returns the current |persistent_storage_manager_|.
  BreadcrumbPersistentStorageManager* GetPersistentStorageManager();

 private:
  // A short string identifying the browser used to initialize the receiver. For
  // example, "I" for "I"ncognito browsing mode. This value is prepended to
  // events sent to |AddEvent| in order to differentiate the browser associated
  // with each event.
  // Note: Normal browsing mode uses an empty string in order to prevent
  // prepending most events with the same static value.
  std::string browsing_mode_;

  // The associated BreadcrumbManager to store events added with |AddEvent|.
  std::unique_ptr<BreadcrumbManager> breadcrumb_manager_;

  // The current BreadcrumbPersistentStorageManager persisting events logged to
  // |breadcrumb_manager_|, set by StartPersisting. May be null.
  raw_ptr<BreadcrumbPersistentStorageManager> persistent_storage_manager_ =
      nullptr;
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_KEYED_SERVICE_H_
