// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_SOURCE_OBSERVER_H_
#define COMPONENTS_EXO_DATA_SOURCE_OBSERVER_H_

#include "base/observer_list_types.h"

namespace exo {

class DataSource;

// Handles events on data devices in context-specific ways.
class DataSourceObserver : public base::CheckedObserver {
 public:
  // Called at the top of the data device's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnDataSourceDestroying(DataSource* source) = 0;

 protected:
  ~DataSourceObserver() override = default;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_SOURCE_OBSERVER_H_
