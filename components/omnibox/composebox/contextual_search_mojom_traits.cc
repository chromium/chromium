// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/contextual_search_mojom_traits.h"

#include "base/notreached.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"

namespace mojo {

namespace {
using UsedToolMode = composebox_query::mojom::ToolMode;
using UsedModelMode = composebox_query::mojom::ModelMode;
using UsedInputType = composebox_query::mojom::InputType;
}  // namespace

composebox_query::mojom::ToolMode
EnumTraits<composebox_query::mojom::ToolMode, omnibox::ToolMode>::ToMojom(
    omnibox::ToolMode input) {
  switch (input) {
    case omnibox::ToolMode::TOOL_MODE_UNSPECIFIED:
      return UsedToolMode::kUnspecified;
    case omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH:
      return UsedToolMode::kDeepSearch;
    case omnibox::ToolMode::TOOL_MODE_CANVAS:
      return UsedToolMode::kCanvas;
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN:
      return UsedToolMode::kImageGen;
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD:
      return UsedToolMode::kImageGenUpload;
  }
  NOTREACHED();
}

bool EnumTraits<composebox_query::mojom::ToolMode,
                omnibox::ToolMode>::FromMojom(composebox_query::mojom::ToolMode
                                                  input,
                                              omnibox::ToolMode* output) {
  switch (input) {
    case UsedToolMode::kUnspecified:
      *output = omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
      return true;
    case UsedToolMode::kDeepSearch:
      *output = omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH;
      return true;
    case UsedToolMode::kCanvas:
      *output = omnibox::ToolMode::TOOL_MODE_CANVAS;
      return true;
    case UsedToolMode::kDeepBrowse:
      *output = omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
      return true;
    case UsedToolMode::kImageGen:
      *output = omnibox::ToolMode::TOOL_MODE_IMAGE_GEN;
      return true;
    case UsedToolMode::kImageGenSelfie:
      *output = omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
      return true;
    case UsedToolMode::kImageGenUpload:
      *output = omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD;
      return true;
  }
  NOTREACHED();
}

composebox_query::mojom::ModelMode
EnumTraits<composebox_query::mojom::ModelMode, omnibox::ModelMode>::ToMojom(
    omnibox::ModelMode input) {
  switch (input) {
    case omnibox::ModelMode::MODEL_MODE_UNSPECIFIED:
      return UsedModelMode::kUnspecified;
    case omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR:
      return UsedModelMode::kGeminiRegular;
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO:
      return UsedModelMode::kGeminiPro;
    case omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE:
      return UsedModelMode::kGeminiProAutoroute;
  }
  NOTREACHED();
}

bool EnumTraits<composebox_query::mojom::ModelMode, omnibox::ModelMode>::
    FromMojom(composebox_query::mojom::ModelMode input,
              omnibox::ModelMode* output) {
  switch (input) {
    case UsedModelMode::kUnspecified:
      *output = omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;
      return true;
    case UsedModelMode::kGeminiRegular:
      *output = omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR;
      return true;
    case UsedModelMode::kGeminiPro:
      *output = omnibox::ModelMode::MODEL_MODE_GEMINI_PRO;
      return true;
    case UsedModelMode::kGeminiProAutoroute:
      *output = omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE;
      return true;
  }
  NOTREACHED();
}

composebox_query::mojom::InputType
EnumTraits<composebox_query::mojom::InputType, omnibox::InputType>::ToMojom(
    omnibox::InputType input) {
  switch (input) {
    case omnibox::InputType::INPUT_TYPE_UNSPECIFIED:
      return UsedInputType::kUnspecified;
    case omnibox::InputType::INPUT_TYPE_LENS_IMAGE:
      return UsedInputType::kLensImage;
    case omnibox::InputType::INPUT_TYPE_LENS_FILE:
      return UsedInputType::kLensFile;
    default:
      return UsedInputType::kUnspecified;
  }
  NOTREACHED();
}

bool EnumTraits<composebox_query::mojom::InputType, omnibox::InputType>::
    FromMojom(composebox_query::mojom::InputType input,
              omnibox::InputType* output) {
  switch (input) {
    case UsedInputType::kUnspecified:
      *output = omnibox::InputType::INPUT_TYPE_UNSPECIFIED;
      return true;
    case UsedInputType::kLensImage:
      *output = omnibox::InputType::INPUT_TYPE_LENS_IMAGE;
      return true;
    case UsedInputType::kLensFile:
      *output = omnibox::InputType::INPUT_TYPE_LENS_FILE;
      return true;
    case UsedInputType::kBrowserTab:
      *output = omnibox::InputType::INPUT_TYPE_BROWSER_TAB;
      return true;
  }
  NOTREACHED();
}

}  // namespace mojo
