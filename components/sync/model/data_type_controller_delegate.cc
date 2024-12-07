// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/data_type_controller_delegate.h"

namespace syncer {

DataTypeControllerDelegate::DataTypeControllerDelegate() = default;
DataTypeControllerDelegate::~DataTypeControllerDelegate() = default;

base::WeakPtr<DataTypeControllerDelegate>
DataTypeControllerDelegate::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace syncer
