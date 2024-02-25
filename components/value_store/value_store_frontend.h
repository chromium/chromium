// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VALUE_STORE_VALUE_STORE_FRONTEND_H_
#define COMPONENTS_VALUE_STORE_VALUE_STORE_FRONTEND_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace value_store {
class ValueStoreFactory;

// A frontend for a LeveldbValueStore, for use on the UI thread.
class ValueStoreFrontend {
 public:
  using ReadCallback = base::OnceCallback<void(std::optional<base::Value>)>;

  ValueStoreFrontend(
      const scoped_refptr<ValueStoreFactory>& store_factory,
      const base::FilePath& directory,
      const std::string& uma_client_name,
      const scoped_refptr<base::SequencedTaskRunner>& origin_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& file_task_runner);
  ~ValueStoreFrontend();
  ValueStoreFrontend(const ValueStoreFrontend&) = delete;
  ValueStoreFrontend& operator=(const ValueStoreFrontend&) = delete;

  // Retrieves a value from the database asynchronously, passing a copy to
  // |callback| when ready. NULL is passed if no matching entry is found.
  void Get(const std::string& key, ReadCallback callback);

  // Sets a value with the given key.
  void Set(const std::string& key, base::Value value);

  // Removes the value with the given key.
  void Remove(const std::string& key);

 private:
  class Backend;

  // A helper class to manage lifetime of the backing ValueStore, which lives
  // on the FILE thread.
  scoped_refptr<Backend> backend_;

  // The task runner on which to fire callbacks.
  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
};

}  // namespace value_store

#endif  // COMPONENTS_VALUE_STORE_VALUE_STORE_FRONTEND_H_
