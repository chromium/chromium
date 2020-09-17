// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_RENDERER_ID_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_RENDERER_ID_H_

#include <stdint.h>
#include <limits>

#include "base/util/type_safety/id_type.h"

namespace autofill {

// A value that can be provided by the renderer in cases when the ID is missing.
constexpr int kNotSetRendererID = -1;

namespace internal {

using FormRendererIdType = ::util::IdType<class FormRendererIdMarker,
                                          uint32_t,
                                          std::numeric_limits<uint32_t>::max()>;

using FieldRendererIdType =
    ::util::IdType<class FieldRendererIdMarker,
                   uint32_t,
                   std::numeric_limits<uint32_t>::max()>;

}  // namespace internal

// The below strong aliases are defined as subclasses instead of typedefs in
// order to avoid having to define out-of-line constructors in all structs that
// contain renderer IDs.

// The FormRendererId of a synthetic form is_null(). A synthetic form is the
// collection of form fields outside of the scope of any <form> tag in a page.
class FormRendererId : public internal::FormRendererIdType {
  using internal::FormRendererIdType::IdType;
};

class FieldRendererId : public internal::FieldRendererIdType {
  using internal::FieldRendererIdType::IdType;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_RENDERER_ID_H_
