// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_UI_STRING_OVERRIDER_H_
#define COMPONENTS_VARIATIONS_SERVICE_UI_STRING_OVERRIDER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"

namespace variations {

// Provides a mapping from hashes of generated resource names to their IDs. The
// mapping is provided by the embedder as two arrays |resource_hashes|, a sorted
// array of resource name hashes, and |resource_indices| an array of resource
// indices in the same order.
//
// The mapping is created by generate_ui_string_overrider.py based on generated
// resources header files. The script ensure that if one header file contains
// |#define IDS_FOO 12345| then for some index |i|, |resource_hashes[i]| will
// be equal to |HASH("IDS_FOO")| and |resource_indices[i]| will be equal to
// |12345|.
//
// Both array must have the same length |num_resources|. They are not owned by
// the UIStringOverrider and the embedder is responsible for their lifetime
// (usually by passing pointer to static data).
//
// This class is copy-constructible by design as it does not owns the array
// and only have reference to globally allocated constants.
class UIStringOverrider {
 public:
  UIStringOverrider();
  UIStringOverrider(base::span<const uint32_t> resource_hashes,
                    base::span<const int> resource_indices);

  UIStringOverrider(const UIStringOverrider&);

  UIStringOverrider& operator=(const UIStringOverrider&) = delete;

  ~UIStringOverrider();

  // Returns the resource index corresponding to the given hash or -1 if no
  // resources is found. Visible for testing.
  int GetResourceIndex(uint32_t hash);

 private:
  const base::raw_span<const uint32_t> resource_hashes_;
  const base::raw_span<const int> resource_indices_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_UI_STRING_OVERRIDER_H_
