// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_UNIQUE_IDS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_UNIQUE_IDS_H_

#include <stdint.h>
#include <limits>
#include <ostream>

#include "base/types/id_type.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill {

namespace internal {

// TokenType wraps an base::UnguessableToken just like base::TokenType but
// initializes to zero by default. We use it to define our own versions of
// LocalFrameToken and RemoteFrameToken to avoid dependencies on blink here and
// in the mojo code, since iOS depends on this code.
template <typename TokenTypeMarker>
class TokenType
    : public base::StrongAlias<TokenTypeMarker, base::UnguessableToken> {
 public:
  using base::StrongAlias<TokenTypeMarker, base::UnguessableToken>::StrongAlias;
  bool is_empty() const { return this->value().is_empty(); }
  explicit constexpr operator bool() const { return !is_empty(); }
  std::string ToString() const { return this->value().ToString(); }
};

}  // namespace internal

// LocalFrameToken and RemoteFrameToken identifiers of frames. LocalFrameToken
// is the unique identifier of the frame, which changes upon a cross-origin
// navigation in that frame. A RemoteFrameToken is an identifier used by a
// renderer process for a frame in a different renderer process.
//
// FrameTokens are not necessarily persistent across page loads.
//
// They serve the same purpose as blink::LocalFrameToken and
// blink::RemoteFrameToken, but avoid dependencies on blink since this code is
// shared with iOS. Also, they default-initialize to zero instead of a random
// number.
//
// They must not be leaked to renderer processes other than the one they
// originate from, so Autofill should generally not send them to any renderer
// process.
using RemoteFrameToken = internal::TokenType<class RemoteFrameTokenMarker>;
using LocalFrameToken = internal::TokenType<class LocalFrameTokenMarker>;
using FrameToken = absl::variant<RemoteFrameToken, LocalFrameToken>;

namespace internal {

#if BUILDFLAG(IS_IOS)
using FormRendererIdType = ::base::IdTypeU32<class FormRendererIdMarker>;
using FieldRendererIdType = ::base::IdTypeU32<class FieldRendererIdMarker>;
#else
using FormRendererIdType = ::base::IdTypeU64<class FormRendererIdMarker>;
using FieldRendererIdType = ::base::IdTypeU64<class FieldRendererIdMarker>;
#endif

}  // namespace internal

// FormRendererId and FieldRendererId uniquely identify a DOM form or field
// element, respectively, among all such elements in one frame.
//
// To uniquely identify frames across frames, see FormGlobalId and
// FieldGlobalId.
//
// As a sentinel value, the FormRendererId of a synthetic form converts to
// `false` (== is_null()). A synthetic form is the collection of form fields
// outside of the scope of any <form> tag in a page.
//
// Since each page can trigger an overflow, security must not rely on their
// uniqueness.
//
// RendererIds are not necessarily persistent across page loads.
//
// The types are defined as subclasses instead of typedefs in order to avoid
// having to define out-of-line constructors in all structs that contain
// renderer IDs.
class FormRendererId : public internal::FormRendererIdType {
  using internal::FormRendererIdType::IdType;
};
class FieldRendererId : public internal::FieldRendererIdType {
  using internal::FieldRendererIdType::IdType;
};

namespace internal {

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

// FormGlobalId and FieldGlobalId uniquely identify a DOM form or field
// element, respectively, among all such elements in all frames.
//
// As a sentinel value, the FormRendererId of a synthetic form converts to
// `false`. A synthetic form is the collection of form fields outside of the
// scope of any <form> tag in a page.
//
// GlobalIds are not necessarily persistent across page loads.
//
// Since LocalFrameTokens must not be leaked to renderer processes other than
// the one they originate from, so Autofill should generally not send GlobalIds
// to any renderer process.
//
// TODO(crbug/1207920) Move to core/browser.
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
