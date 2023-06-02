// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MODEL_TYPE_STORE_TEST_UTIL_H_
#define COMPONENTS_SYNC_TEST_MODEL_TYPE_STORE_TEST_UTIL_H_

#include <memory>

#include "components/sync/base/storage_type.h"
#include "components/sync/model/model_type_store.h"

namespace syncer {

// Util class with several static methods to facilitate writing unit tests for
// classes that use ModelTypeStore objects.
class ModelTypeStoreTestUtil {
 public:
  // Creates an in memory store synchronously.
  static std::unique_ptr<ModelTypeStore> CreateInMemoryStoreForTest(
      ModelType type = PREFERENCES,
      StorageType storage_type = StorageType::kUnspecified);

  // Creates a factory callback to synchronously return in memory stores.
  static RepeatingModelTypeStoreFactory FactoryForInMemoryStoreForTest();

  // Returns a once-factory that returns an already created store to a service
  // constructor in a unit test.
  static OnceModelTypeStoreFactory MoveStoreToFactory(
      std::unique_ptr<ModelTypeStore> store);

  // Returns a callback that constructs a store that forwards all calls to
  // |target|. |*target| must outlive the returned factory as well any store
  // created by the factory.
  static RepeatingModelTypeStoreFactory FactoryForForwardingStore(
      ModelTypeStore* target);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MODEL_TYPE_STORE_TEST_UTIL_H_
