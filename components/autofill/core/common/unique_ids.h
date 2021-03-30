// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_UNIQUE_IDS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_UNIQUE_IDS_H_

#include <stdint.h>
#include <limits>
#include <ostream>

#include "base/unguessable_token.h"
#include "base/util/type_safety/id_type.h"

namespace autofill {

// LocalFrameToken is a unique identifier of a frame. The type is essentially
// identical to blink::LocalFrameToken, except that the default constructor
// initializes to zero instead of creating a new randomly generated token.
// The purpose of this duplicate is to avoid dependencies on blink here and in
// the mojo code, since iOS depends on the code.
class LocalFrameToken : public base::StrongAlias<class LocalFrameTokenMarker,
                                                 base::UnguessableToken> {
 public:
  using base::StrongAlias<class LocalFrameTokenMarker,
                          base::UnguessableToken>::StrongAlias;
  bool is_empty() const { return value().is_empty(); }
  std::string ToString() const { return value().ToString(); }
};

namespace internal {

using FormRendererIdType = ::util::IdTypeU32<class FormRendererIdMarker>;

using FieldRendererIdType = ::util::IdTypeU32<class FieldRendererIdMarker>;

template <typename RendererId>
struct GlobalId {
  LocalFrameToken frame_token;
  RendererId renderer_id;

  // Not all platforms work with multiple frames and set the
  // FormData::host_frame and FormFieldData::host_frame yet.
  // Therefore, only check |renderer_id|.
  explicit constexpr operator bool() const {
    return static_cast<bool>(renderer_id);
  }
};

template <typename RendererId>
bool operator==(const GlobalId<RendererId>& a, const GlobalId<RendererId>& b) {
  return a.renderer_id == b.renderer_id && a.frame_token == b.frame_token;
}

template <typename RendererId>
bool operator!=(const GlobalId<RendererId>& a, const GlobalId<RendererId>& b) {
  return !(a == b);
}

template <typename RendererId>
bool operator<(const GlobalId<RendererId>& a, const GlobalId<RendererId>& b) {
  return std::tie(a.frame_token, a.renderer_id) <
         std::tie(b.frame_token, b.renderer_id);
}

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

using FormGlobalId = internal::GlobalId<FormRendererId>;
using FieldGlobalId = internal::GlobalId<FieldRendererId>;

class LogBuffer;

std::ostream& operator<<(std::ostream& os, const FormRendererId& form);
std::ostream& operator<<(std::ostream& os, const FieldRendererId& field);
std::ostream& operator<<(std::ostream& os, const FormGlobalId& form);
std::ostream& operator<<(std::ostream& os, const FieldGlobalId& field);
LogBuffer& operator<<(LogBuffer& buffer, const FormRendererId& form);
LogBuffer& operator<<(LogBuffer& buffer, const FieldRendererId& field);
LogBuffer& operator<<(LogBuffer& buffer, const FormGlobalId& form);
LogBuffer& operator<<(LogBuffer& buffer, const FieldGlobalId& field);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_UNIQUE_IDS_H_
