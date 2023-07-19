// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_LOCAL_STORAGE_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_LOCAL_STORAGE_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/storage_usage_info.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {
class StoragePartition;
}  // namespace content

namespace browsing_data {

// This class fetches local storage information and provides a
// means to delete the data associated with an StorageKey.
class LocalStorageHelper : public base::RefCounted<LocalStorageHelper> {
 public:
  using FetchCallback =
      base::OnceCallback<void(const std::list<content::StorageUsageInfo>&)>;

  explicit LocalStorageHelper(content::StoragePartition* storage_partition);

  LocalStorageHelper(const LocalStorageHelper&) = delete;
  LocalStorageHelper& operator=(const LocalStorageHelper&) = delete;

  // Starts the fetching process, which will notify its completion via
  // callback. This must be called only in the UI thread.
  virtual void StartFetching(FetchCallback callback);

  // Deletes the local storage for the `storage_key`. `callback` is called when
  // the deletion is sent to the database and `StartFetching()` doesn't return
  // entries for `storage_key` anymore.
  virtual void DeleteStorageKey(const blink::StorageKey& storage_key,
                                base::OnceClosure callback);

 protected:
  friend class base::RefCounted<LocalStorageHelper>;
  virtual ~LocalStorageHelper();

  raw_ptr<content::DOMStorageContext, AcrossTasksDanglingUntriaged>
      dom_storage_context_;  // Owned by the context
};

// This class is a thin wrapper around LocalStorageHelper that does not fetch
// its information from the local storage context, but gets them passed by a
// call when accessed.
//   If `update_ignored_empty_keys_on_fetch` is true, `UpdateIgnoredEmptyKeys`
// is automatically called when `StartFetching` is called. But note that
// `UpdateIgnoredEmptyKeys` must still be manually called before calling the
// other methods such as `GetCount`, `empty` and `GetStorageKeys`.
class CannedLocalStorageHelper : public LocalStorageHelper {
 public:
  explicit CannedLocalStorageHelper(
      content::StoragePartition* storage_partition,
      bool update_ignored_empty_keys_on_fetch = false);

  CannedLocalStorageHelper(const CannedLocalStorageHelper&) = delete;
  CannedLocalStorageHelper& operator=(const CannedLocalStorageHelper&) = delete;

  // Add a local storage to the set of canned local storages that is returned
  // by this helper.
  void Add(const blink::StorageKey& storage_key);

  // Clear the list of canned local storages.
  void Reset();

  // True if no local storages are currently stored.
  bool empty() const;

  // Returns the number of local storages currently stored.
  size_t GetCount() const;

  // Returns the set of StorageKeys that use local storage.
  const std::set<blink::StorageKey>& GetStorageKeys() const;

  // Update the list of empty StorageKeys to ignore.
  // Note: If `update_ignored_empty_keys_on_fetch` is true this is also called
  //       by the `StartFetching` method automatically.
  void UpdateIgnoredEmptyKeys(base::OnceClosure done);

  // LocalStorageHelper implementation.
  void StartFetching(FetchCallback callback) override;
  void DeleteStorageKey(const blink::StorageKey& storage_key,
                        base::OnceClosure callback) override;

 private:
  ~CannedLocalStorageHelper() override;

  std::set<blink::StorageKey> pending_storage_keys_;
  std::set<blink::StorageKey> non_empty_pending_storage_keys_;
  bool update_ignored_empty_keys_on_fetch_ = false;

  void UpdateIgnoredEmptyKeysInternal(
      base::OnceClosure done,
      const std::list<content::StorageUsageInfo>& storage_usage_info);

  void StartFetchingInternal(FetchCallback callback);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_LOCAL_STORAGE_HELPER_H_
