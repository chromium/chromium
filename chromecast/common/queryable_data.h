// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_QUERYABLE_DATA_H_
#define CHROMECAST_COMMON_QUERYABLE_DATA_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/values.h"

namespace chromecast {

// This class is for use by both browser- and renderer-side, and tracks data
// that can be exposed directly to applications via JS APIs. The browser-side
// code uses it to expose key/value to renderer. The ipc to renderer is done by
// binding QueryableDataStorePointer on browser side to the implementation on
// the renderer side and calling the Set method.
// All methods should be accessed on the main thread.
// TODO(mdellaquila): Change the class implementation so that an instance of it
// is created for each RenderFrame
class QueryableData {
 public:
  using ValueMap = base::flat_map<std::string, base::Value>;

  // Stores a value for the current process. If the key already exists, the
  // value is replaced.
  static void RegisterQueryableValue(const std::string& query_key,
                                     base::Value initial_value);

  // Returns a pointer to the value for given key, if found, or null if not.
  static const base::Value* Query(const std::string& query_key);

  static const ValueMap& GetValues();

 private:
  friend class base::NoDestructor<QueryableData>;

  QueryableData();
  ~QueryableData();

  SEQUENCE_CHECKER(sequence_checker_);
  ValueMap queryable_values_;

  DISALLOW_COPY_AND_ASSIGN(QueryableData);
};

}  // namespace chromecast

#endif  //  CHROMECAST_COMMON_QUERYABLE_DATA_H_
