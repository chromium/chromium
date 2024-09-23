// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_UNIQUE_IDS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_UNIQUE_IDS_H_

#include <ostream>
#include <string>

#include "base/types/id_type.h"
#include "base/types/strong_alias.h"
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

// LocalFrameToken and RemoteFrameToken identify AutofillDrivers and
// AutofillAgents.
//
// TODO(crbug.com/40266699): Implement frame tokens as described below for iOS.
//
// Every pair of associated AutofillAgent and AutofillDriver has a
// LocalFrameToken, which uniquely identifies them and remains stable for their
// lifetime.
//
// In the //content layer, LocalFrameTokens are a secret between the browser
// process and the renderer process hosting the respective AutofillAgent.
// Therefore, cross-process AutofillAgents do not know each other's
// LocalFrameToken.
//
// In such a case, an AutofillAgent A1 refers to another AutofillAgent A2 by
// a RemoteFrameToken. The associated AutofillDriver D1 can obtain the
// LocalFrameToken of A2 and its associated AutofillDriver D2 by calling
// AutofillDriver::Resolve(). RemoteFrameTokens only have weak properties:
//
// 1. Different AutofillAgents may refer to A2 by different RemoteFrameTokens.
//    That is, D1.Resolve(R) == D3.Resolve(R') is possible for distinct
//    RemoteFrameTokens R, R', especially when A1 and A3 are hosted by different
//    renderer processes.
//    Therefore, RemoteFrameTokens are not suitable for discriminating between
//    AutofillAgents and AutofillDrivers in the browser process.
//
// 2. Over time, the same RemoteFrameToken may resolve to different
//    LocalFrameTokens. Suppose a navigation in A2's frame replaces A2 and D2
//    with a new objects AutofillAgent A3 and AutofillDriver D3 (this happens on
//    a renderer process swap, which typically happens on cross-origin
//    navigations). Then A2 and A3 have distinct LocalFrameTokens, but A1 may
//    refer to A3 by the same RemoteFrameToken as it did to A2. That is,
//    D1.Resolve(R) may change over cross-origin navigations.
//    Therefore, AutofillDriver::Resolve() must not be cached.
//
// LocalFrameToken and RemoteFrameToken are Blink and //content layer concepts.
//
// If the //content layer is available, AutofillAgent and AutofillDriver inherit
// their {Local,Remote}FrameToken from blink::Web{Local,Remote}Frame and
// content::RenderFrameHost.
//
// On iOS, AutofillAgent and AutofillDriver inherit their LocalFrameToken from
// web::WebFrame, and RemoteFrameTokens are generated during form extraction.
//
// FrameTokens must not be leaked to renderer processes other than the one
// they originate from. Therefore, Autofill should generally not send
// FrameTokens to any renderer process.
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
// To uniquely identify forms and fields across frames, see FormGlobalId and
// FieldGlobalId.
//
// As a sentinel value, the FormRendererId of a synthetic form converts to
// `false` (== is_null()). A synthetic form is the collection of form fields
// outside of the scope of any <form> tag in a document.
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

  friend auto operator<=>(const GlobalId<RendererId>& lhs,
                          const GlobalId<RendererId>& rhs) = default;
  friend bool operator==(const GlobalId<RendererId>& lhs,
                         const GlobalId<RendererId>& rhs) = default;
};

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
// the one they originate from, Autofill does not send GlobalIds to any renderer
// process.
//
// TODO(crbug.com/40181498) Move to core/browser.
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
