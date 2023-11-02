// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULE_PARSER_RULE_OPTIONS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULE_PARSER_RULE_OPTIONS_H_

#include <limits>

#include "components/url_pattern_index/proto/rules.pb.h"
#include "components/url_pattern_index/url_pattern_index.h"

namespace subresource_filter {

// A type used for representing bitmask with ElementType's and ActivationType's.
using TypeMask = uint32_t;

static constexpr uint32_t kActivationTypesShift = 24;
static constexpr uint32_t kActivationTypesBits =
    std::numeric_limits<TypeMask>::digits - kActivationTypesShift;

static_assert(kActivationTypesShift <= std::numeric_limits<TypeMask>::digits,
              "TypeMask layout is broken");
static_assert(url_pattern_index::proto::ElementType_MAX <
                  (1 << kActivationTypesShift),
              "TypeMask layout is broken");
static_assert(url_pattern_index::proto::ActivationType_MAX <
                  (1 << kActivationTypesBits),
              "TypeMask layout is broken");

// The functions used to calculate masks for individual types.
inline constexpr TypeMask type_mask_for(
    url_pattern_index::proto::ElementType type) {
  return type;
}
inline constexpr TypeMask type_mask_for(
    url_pattern_index::proto::ActivationType type) {
  return type << kActivationTypesShift;
}

static constexpr TypeMask kAllElementTypes =
    type_mask_for(url_pattern_index::proto::ELEMENT_TYPE_ALL);
static constexpr TypeMask kAllActivationTypes =
    type_mask_for(url_pattern_index::proto::ACTIVATION_TYPE_ALL);

static constexpr TypeMask kDefaultElementTypes =
    url_pattern_index::kDefaultProtoElementTypesMask;

// A list of items mapping element type options to their names.
const struct {
  url_pattern_index::proto::ElementType type;
  const char* name;
} kElementTypes[] = {
    {url_pattern_index::proto::ELEMENT_TYPE_OTHER, "other"},
    {url_pattern_index::proto::ELEMENT_TYPE_SCRIPT, "script"},
    {url_pattern_index::proto::ELEMENT_TYPE_IMAGE, "image"},
    {url_pattern_index::proto::ELEMENT_TYPE_STYLESHEET, "stylesheet"},
    {url_pattern_index::proto::ELEMENT_TYPE_OBJECT, "object"},
    {url_pattern_index::proto::ELEMENT_TYPE_XMLHTTPREQUEST, "xmlhttprequest"},
    {url_pattern_index::proto::ELEMENT_TYPE_OBJECT_SUBREQUEST,
     "object-subrequest"},
    {url_pattern_index::proto::ELEMENT_TYPE_SUBDOCUMENT, "subdocument"},
    {url_pattern_index::proto::ELEMENT_TYPE_PING, "ping"},
    {url_pattern_index::proto::ELEMENT_TYPE_MEDIA, "media"},
    {url_pattern_index::proto::ELEMENT_TYPE_FONT, "font"},
    {url_pattern_index::proto::ELEMENT_TYPE_POPUP, "popup"},
    {url_pattern_index::proto::ELEMENT_TYPE_WEBSOCKET, "websocket"},
    // This is currently not used by EasyList. If EasyList adds blocking support
    // for webtransport or webbundle, make sure it is compatible with this
    // spelling.
    {url_pattern_index::proto::ELEMENT_TYPE_WEBTRANSPORT, "webtransport"},
    {url_pattern_index::proto::ELEMENT_TYPE_WEBBUNDLE, "webbundle"},
};

// A mapping from deprecated element type names to active element types.
const struct {
  const char* name;
  url_pattern_index::proto::ElementType maps_to_type;
} kDeprecatedElementTypes[] = {
    {"background", url_pattern_index::proto::ELEMENT_TYPE_IMAGE},
    {"xbl", url_pattern_index::proto::ELEMENT_TYPE_OTHER},
    {"dtd", url_pattern_index::proto::ELEMENT_TYPE_OTHER},
};

// A list of items mapping activation type options to their names.
const struct {
  url_pattern_index::proto::ActivationType type;
  const char* name;
} kActivationTypes[] = {
    {url_pattern_index::proto::ACTIVATION_TYPE_DOCUMENT, "document"},
    {url_pattern_index::proto::ACTIVATION_TYPE_ELEMHIDE, "elemhide"},
    {url_pattern_index::proto::ACTIVATION_TYPE_GENERICHIDE, "generichide"},
    {url_pattern_index::proto::ACTIVATION_TYPE_GENERICBLOCK, "genericblock"},
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULE_PARSER_RULE_OPTIONS_H_
