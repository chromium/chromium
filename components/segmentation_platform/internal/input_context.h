// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_INPUT_CONTEXT_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_INPUT_CONTEXT_H_

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"

namespace segmentation_platform {

class SegmentationTabHelper;

// Experimental API, DO NOT USE.
// Input provided for segment selection, based on the current state of the
// browser.
struct InputContext : base::RefCounted<InputContext> {
 public:
  InputContext();

  InputContext(InputContext&) = delete;
  InputContext& operator=(InputContext&) = delete;

  // Input values that can be used to input to model directly if the type is
  // `Type::INTEGER` or `Type::DOUBLE`. Inputs can be substituted to SQL queries
  // if the type is not `Type::DICT` or `Type::LIST`.
  base::flat_map<std::string, base::Value> metadata_args;

 private:
  friend class RefCounted<InputContext>;

  ~InputContext();
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_INPUT_CONTEXT_H_
