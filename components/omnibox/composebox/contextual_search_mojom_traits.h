// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMPOSEBOX_CONTEXTUAL_SEARCH_MOJOM_TRAITS_H_
#define COMPONENTS_OMNIBOX_COMPOSEBOX_CONTEXTUAL_SEARCH_MOJOM_TRAITS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "components/omnibox/common/input_state.h"
#include "components/omnibox/composebox/composebox_query.mojom-forward.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/model_config.pb.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/section_config.pb.h"
#include "third_party/omnibox_proto/tool_config.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"

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

template <>
struct StructTraits<composebox_query::mojom::ToolConfigDataView,
                    omnibox::ToolConfig> {
  static omnibox::ToolMode tool(const omnibox::ToolConfig& config);
  static bool disable_active_model_selection(const omnibox::ToolConfig& config);
  static const std::string& menu_label(const omnibox::ToolConfig& config);
  static const std::string& chip_label(const omnibox::ToolConfig& config);
  static const std::string& hint_text(const omnibox::ToolConfig& config);

  static bool Read(composebox_query::mojom::ToolConfigDataView data,
                   omnibox::ToolConfig* output);
};

template <>
struct StructTraits<composebox_query::mojom::ModelConfigDataView,
                    omnibox::ModelConfig> {
  static omnibox::ModelMode model(const omnibox::ModelConfig& config);
  static const std::string& menu_label(const omnibox::ModelConfig& config);
  static const std::string& hint_text(const omnibox::ModelConfig& config);

  static bool Read(composebox_query::mojom::ModelConfigDataView data,
                   omnibox::ModelConfig* output);
};

template <>
struct StructTraits<composebox_query::mojom::SectionConfigDataView,
                    omnibox::SectionConfig> {
  static const std::string& header(const omnibox::SectionConfig& config);

  static bool Read(composebox_query::mojom::SectionConfigDataView data,
                   omnibox::SectionConfig* output);
};

template <>
struct StructTraits<composebox_query::mojom::InputStateDataView,
                    omnibox::InputState> {
  static const std::vector<omnibox::ModelMode>& allowed_models(
      const omnibox::InputState& input);
  static const std::vector<omnibox::ToolMode>& allowed_tools(
      const omnibox::InputState& input);
  static const std::vector<omnibox::InputType>& allowed_input_types(
      const omnibox::InputState& input);
  static omnibox::ModelMode active_model(const omnibox::InputState& input);
  static omnibox::ToolMode active_tool(const omnibox::InputState& input);
  static const std::vector<omnibox::ModelMode>& disabled_models(
      const omnibox::InputState& input);
  static const std::vector<omnibox::ToolMode>& disabled_tools(
      const omnibox::InputState& input);
  static const std::vector<omnibox::InputType>& disabled_input_types(
      const omnibox::InputState& input);
  static const std::vector<omnibox::ToolConfig>& tool_configs(
      const omnibox::InputState& input);
  static const std::vector<omnibox::ModelConfig>& model_configs(
      const omnibox::InputState& input);
  static const std::optional<omnibox::SectionConfig>& tools_section_config(
      const omnibox::InputState& input);
  static const std::optional<omnibox::SectionConfig>& model_section_config(
      const omnibox::InputState& input);
  static const std::string& hint_text(const omnibox::InputState& input);

  static bool Read(composebox_query::mojom::InputStateDataView data,
                   omnibox::InputState* output);
};

}  // namespace mojo

#endif  // COMPONENTS_OMNIBOX_COMPOSEBOX_CONTEXTUAL_SEARCH_MOJOM_TRAITS_H_
