// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_FILTER_H_
#define COMPONENTS_PREFS_PREF_FILTER_H_

#include <string_view>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "components/prefs/prefs_export.h"

// Filters preferences as they are loaded from disk or updated at runtime.
// Currently supported only by JsonPrefStore.
class COMPONENTS_PREFS_EXPORT PrefFilter {
 public:
  // A pair of pre-write and post-write callbacks.
  using OnWriteCallbackPair =
      std::pair<base::OnceClosure, base::OnceCallback<void(bool success)>>;

  // A callback to be invoked when |prefs| have been read (and possibly
  // pre-modified) and are now ready to be handed back to this callback's
  // builder. |schedule_write| indicates whether a write should be immediately
  // scheduled (typically because the |prefs| were pre-modified).
  using PostFilterOnLoadCallback =
      base::OnceCallback<void(base::Value::Dict prefs, bool schedule_write)>;

  virtual ~PrefFilter() = default;

  // This method is given ownership of the |pref_store_contents| read from disk
  // before the underlying PersistentPrefStore gets to use them. It must hand
  // them back via |post_filter_on_load_callback|, but may modify them first.
  // Note: This method is asynchronous, which may make calls like
  // PersistentPrefStore::ReadPrefs() asynchronous. The owner of filtered
  // PersistentPrefStores should handle this to make the reads look synchronous
  // to external users (see SegregatedPrefStore::ReadPrefs() for an example).
  virtual void FilterOnLoad(
      PostFilterOnLoadCallback post_filter_on_load_callback,
      base::Value::Dict pref_store_contents) = 0;

  // Receives notification when a pref store value is changed, before Observers
  // are notified.
  virtual void FilterUpdate(std::string_view path) = 0;

  // Receives notification when the pref store is about to serialize data
  // contained in |pref_store_contents| to a string. Modifications to
  // |pref_store_contents| will be persisted to disk and also affect the
  // in-memory state.
  // If the returned callbacks are non-null, they will be registered to be
  // invoked synchronously after the next write (from the I/O TaskRunner so they
  // must not be bound to thread-unsafe member state).
  virtual OnWriteCallbackPair FilterSerializeData(
      base::Value::Dict& pref_store_contents) = 0;

  // Cleans preference data that may have been saved outside of the store.
  virtual void OnStoreDeletionFromDisk() = 0;
};

#endif  // COMPONENTS_PREFS_PREF_FILTER_H_
