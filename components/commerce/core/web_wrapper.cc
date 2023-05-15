// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/web_wrapper.h"

namespace commerce {

WebWrapper::WebWrapper() = default;

WebWrapper::~WebWrapper() = default;

base::WeakPtr<WebWrapper> WebWrapper::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace commerce
