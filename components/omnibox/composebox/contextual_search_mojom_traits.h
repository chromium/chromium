// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMPOSEBOX_CONTEXTUAL_SEARCH_MOJOM_TRAITS_H_
#define COMPONENTS_OMNIBOX_COMPOSEBOX_CONTEXTUAL_SEARCH_MOJOM_TRAITS_H_

#include "components/omnibox/composebox/composebox_query.mojom-forward.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "third_party/omnibox_proto/aim_input_types.pb.h"
#include "third_party/omnibox_proto/aim_models.pb.h"
#include "third_party/omnibox_proto/aim_tools.pb.h"

namespace mojo {

template <>
struct EnumTraits<composebox_query::mojom::ToolMode, omnibox::ToolMode> {
  static composebox_query::mojom::ToolMode ToMojom(omnibox::ToolMode input);
  static bool FromMojom(composebox_query::mojom::ToolMode input,
                        omnibox::ToolMode* output);
};

template <>
struct EnumTraits<composebox_query::mojom::ModelMode, omnibox::ModelMode> {
  static composebox_query::mojom::ModelMode ToMojom(omnibox::ModelMode input);
  static bool FromMojom(composebox_query::mojom::ModelMode input,
                        omnibox::ModelMode* output);
};

template <>
struct EnumTraits<composebox_query::mojom::InputType, omnibox::InputType> {
  static composebox_query::mojom::InputType ToMojom(omnibox::InputType input);
  static bool FromMojom(composebox_query::mojom::InputType input,
                        omnibox::InputType* output);
};

}  // namespace mojo

#endif  // COMPONENTS_OMNIBOX_COMPOSEBOX_CONTEXTUAL_SEARCH_MOJOM_TRAITS_H_
