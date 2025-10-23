// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_POSITION_TRAITS_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_POSITION_TRAITS_H_

#include "components/browser_apis/tab_strip/tab_strip_api_types.mojom.h"
#include "components/browser_apis/tab_strip/types/position.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

// This adapter layer tells Mojo how to handle serialization and deserialization
// of a custom C++ type. It defines traits for converting between a Mojom struct
// "tabs_api::mojom::Position" to the native C++ struct "tabs_api::Position"
//
// Type alias for the mojom dataview.
using MojoPositionView = tabs_api::mojom::PositionDataView;
// Typealias for the native C++ struct.
using NativePosition = tabs_api::Position;

// Position Struct mapping.
template <>
struct mojo::StructTraits<MojoPositionView, NativePosition> {
  // Field getters:
  static const std::optional<tabs_api::NodeId>& parent_id(
      const NativePosition& native);
  static uint32_t index(const NativePosition& native);

  // Decoder:
  static bool Read(MojoPositionView view, NativePosition* out);
};

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_POSITION_TRAITS_H_
