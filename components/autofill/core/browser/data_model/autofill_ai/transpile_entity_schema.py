#!/usr/bin/env python

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates C++ code from an Autofill AI schema definition.

See go/forms-ai:data-model.
TODO(crbug.com/388590912): Add more details.
"""

import argparse
import io
import re
import sys
from entity_schema_parser import parse_entity_schema


# For 'foo-bar' returns 'kFooBar', which is the conventional format of C++
# constants.
def name_to_constant(str):
  return 'k' + ''.join(
    re.sub(r'[^\w\s]', '', s.capitalize()) for s in str.split())


# The enum constant's name of an entity type.
def entity_name(entity, qualified = True):
  prefix = 'EntityTypeName::' if qualified else ''
  return prefix + name_to_constant(entity)


# The enum constant's name of an attribute type.
def attribute_name(entity, attribute, qualified = True):
  prefix = 'AttributeTypeName::' if qualified else ''
  return prefix + name_to_constant(entity +' '+ attribute)


# Adds to list of unqualified enum constant values a `kMaxValue` constant.
def add_kMaxValue(constants):
  constants = list(constants)
  return constants + ['kMaxValue = '+ constants[-1]]


# An expression that creates a DenseSet of attribute types.
def attribute_dense_set(entity, attributes):
  names = (attribute_name(entity, attribute) for attribute in attributes)
  return f'DenseSet<AttributeType>{{{", ".join((f"AttributeType({name})" for name in names))}}}'


# An expression that creates an array of DenseSets of attribute types.
def attribute_dense_set_array(entity, attributes_sets):
  dense_sets = [attribute_dense_set(entity, attributes) for attributes in attributes_sets]
  return f'std::array<DenseSet<AttributeType>, {len(dense_sets)}>{{{", ".join(dense_sets)}}}'


# Generates entity and attribute type name enum definitions.
def generate_cpp_enums(schema):
  entities = (entity_name(entity['name'], qualified=False) for entity in schema)
  attributes = (
      attribute_name(entity['name'], attribute, qualified=False)
      for entity in schema
      for attribute in entity['attributes']
  )
  yield f'enum class EntityTypeName {{ {", ".join(add_kMaxValue(entities))} }};'
  yield ''
  yield f'enum class AttributeTypeName {{ {", ".join(add_kMaxValue(attributes))} }};'


# Generates the function implementations.
def generate_cpp_functions(schema):
  yield 'namespace {'
  yield ''
  yield 'constexpr std::string_view AttributeTypeNameToString(AttributeTypeName name) {'
  yield '  switch (name) {'
  for entity, attribute in ((entity['name'], attribute) for entity in schema for attribute in entity['attributes']):
    yield f'    case {attribute_name(entity, attribute)}:'
    yield f'      return "{attribute}";'
  yield '  }'
  yield '  NOTREACHED();'
  yield '}'
  yield ''
  yield 'constexpr std::string_view EntityTypeNameToString(EntityTypeName name) {'
  yield '  switch (name) {'
  for entity in (entity['name'] for entity in schema):
    yield f'    case {entity_name(entity)}:'
    yield f'      return "{entity}";'
  yield '  }'
  yield '  NOTREACHED();'
  yield '}'
  yield ''
  yield '}  // namespace'
  yield ''
  yield 'std::optional<EntityType> StringToEntityType(std::string_view entity_type_name) {'
  yield '  static constexpr auto kMap = base::MakeFixedFlatMap<std::string_view, EntityType>({'
  yield ',\n'.join('    ' + f'{{EntityTypeNameToString({name}), EntityType({name})}}'
                   for name in (entity_name(entity['name']) for entity in schema))
  yield '  });'
  yield '  auto it = kMap.find(entity_type_name);'
  yield '  return it != kMap.end() ? std::optional(it->second) : std::nullopt;'
  yield '}'
  yield ''
  yield 'std::optional<AttributeType> StringToAttributeType(EntityType entity_type, std::string_view attribute_type_name) {'
  yield '  switch (entity_type.name()) {'
  for entity in schema:
    yield f'    case {entity_name(entity["name"])}: {{'
    yield '      static constexpr auto kMap = base::MakeFixedFlatMap<std::string_view, AttributeType>({'
    yield ',\n'.join(f'        {{AttributeTypeNameToString({name}), AttributeType({name})}}'
                     for name in (attribute_name(entity["name"], attribute) for attribute in entity['attributes']))
    yield '      });'
    yield '      auto it = kMap.find(attribute_type_name);'
    yield '      return it != kMap.end() ? std::optional(it->second) : std::nullopt;'
    yield f'    }}'
  yield '  }'
  yield '  return std::nullopt;'
  yield '}'
  yield ''
  yield 'bool AttributeType::is_obfuscated() const {'
  yield '  switch (name_) {'
  for entity, attribute in ((entity, attribute) for entity in schema for attribute in entity['attributes']):
    yield f'    case {attribute_name(entity["name"], attribute)}:'
    is_obfuscated = attribute in entity['obfuscated attributes']
    yield f'      return {"true" if is_obfuscated else "false"};'
  yield '  }'
  yield '  NOTREACHED();'
  yield '}'
  yield ''
  yield 'std::string_view AttributeType::name_as_string() const {'
  yield '  return AttributeTypeNameToString(name_);'
  yield '}'
  yield ''
  yield 'bool AttributeType::is_disambiguation_type() const {'
  yield '  switch (name_) {'
  for entity in schema:
    for attribute in entity["disambiguation order"]:
      yield f'    case {attribute_name(entity["name"], attribute)}:'
      yield f'      return true;'
  yield f'    default:'
  yield f'      return false;'
  yield '  }'
  yield '}'
  yield ''
  yield 'EntityType AttributeType::entity_type() const {'
  yield '  switch (name_) {'
  for entity, attribute in ((entity['name'], attribute) for entity in schema for attribute in entity['attributes']):
    yield f'    case {attribute_name(entity, attribute)}:'
    yield f'      return EntityType({entity_name(entity)});'
  yield '  }'
  yield '  NOTREACHED();'
  yield '}'
  yield ''
  yield 'std::string_view EntityType::name_as_string() const {'
  yield '  return EntityTypeNameToString(name_);'
  yield '}'
  yield ''
  yield 'DenseSet<AttributeType> EntityType::attributes() const {'
  yield '  switch (name_) {'
  for entity in schema:
    yield f'    case {entity_name(entity["name"])}: {{'
    yield f'      static constexpr auto s = {attribute_dense_set(entity["name"], entity["attributes"])};'
    yield f'      return s;'
    yield f'    }}'
  yield '  }'
  yield '  NOTREACHED();'
  yield '}'
  yield ''
  yield 'base::span<const DenseSet<AttributeType>> EntityType::import_constraints() const {'
  yield '  switch (name_) {'
  for entity in schema:
    yield f'    case {entity_name(entity["name"])}: {{'
    yield f'      static constexpr auto as = {attribute_dense_set_array(entity["name"], entity["import constraints"])};'
    yield f'      return as;'
    yield f'    }}'
  yield '  }'
  yield '  NOTREACHED();'
  yield '}'
  yield ''
  yield 'base::span<const DenseSet<AttributeType>> EntityType::required_fields() const {'
  yield '  switch (name_) {'
  for entity in schema:
    yield f'    case {entity_name(entity["name"])}: {{'
    yield f'      static constexpr auto as = {attribute_dense_set_array(entity["name"], entity["required fields"])};'
    yield f'      return as;'
    yield f'    }}'
  yield '  }'
  yield '  NOTREACHED();'
  yield '}'
  yield ''
  yield 'base::span<const DenseSet<AttributeType>> EntityType::merge_constraints() const {'
  yield '  switch (name_) {'
  for entity in schema:
    yield f'    case {entity_name(entity["name"])}: {{'
    yield f'      static constexpr auto as = {attribute_dense_set_array(entity["name"], entity["merge constraints"])};'
    yield f'      return as;'
    yield f'    }}'
  yield '  }'
  yield '  NOTREACHED();'
  yield '}'
  yield 'base::span<const DenseSet<AttributeType>> EntityType::strike_keys() const {'
  yield '  switch (name_) {'
  for entity in schema:
    yield f'    case {entity_name(entity["name"])}: {{'
    yield f'      static constexpr auto as = {attribute_dense_set_array(entity["name"], entity["strike keys"])};'
    yield f'      return as;'
    yield f'    }}'
  yield '  }'
  yield '  NOTREACHED();'
  yield '}'
  yield ''
  yield 'bool EntityType::enabled(base::optional_ref<const GeoIpCountryCode> country_code) const {'
  yield '  switch (name_) {'
  for entity in schema:
    yield f'    case {entity_name(entity["name"])}: {{'
    feature_name = entity.get('experiment feature', '')
    excluded_geo_ips = entity.get('excluded geo-ips', [])
    if feature_name:
      yield f'      if (!base::FeatureList::IsEnabled(features::k{feature_name})) {{'
      yield '        return false;'
      yield '      }'
    if excluded_geo_ips:
      ip_string = ', '.join(f'"{ip.upper()}"' for ip in excluded_geo_ips)
      yield '      static constexpr auto banned_ips = base::MakeFixedFlatSet<std::string_view>({'
      yield f'          {ip_string}'
      yield '      });'
      yield '      return !country_code || !banned_ips.contains(**country_code);'
    else:
      yield '      return true;'
    yield '    }'
  yield '  }'
  yield '  NOTREACHED();'
  yield '}'
  yield ''
  yield 'bool EntityType::read_only() const {'
  yield '  switch (name_) {'
  for entity, read_only in ((entity['name'], entity['read only'])
                            for entity in schema):
    yield f'    case {entity_name(entity)}:'
    yield f'      return {"true" if read_only else "false"};'
  yield '  }'
  yield '  NOTREACHED();'
  yield '}'
  yield ''
  yield '// static'
  yield 'bool AttributeType::DisambiguationOrder(const AttributeType& lhs, const AttributeType& rhs) {'
  yield '  constexpr auto rank = [](const AttributeType& a) {'
  yield '    static constexpr auto ranks = [] {'
  yield '      std::array<int, base::to_underlying(AttributeTypeName::kMaxValue) + 1> ranks{};'
  yield '      for (int& rank : ranks) {'
  yield '        rank = std::numeric_limits<int>::max();'
  yield '      }'
  for entity, order in ((entity['name'], entity['disambiguation order']) for entity in schema):
    for rank, attribute in enumerate(order):
      yield f'      ranks[base::to_underlying({attribute_name(entity, attribute)})] = {rank+1};'
  yield '      return ranks;'
  yield '    }();'
  yield '    return ranks[base::to_underlying(a.name())];'
  yield '  };'
  yield '  return rank(lhs) < rank(rhs);'
  yield '}'

def generate_cpp_enums_header(schema, include_guard):
  yield f"""// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef {include_guard}
#define {include_guard}

namespace autofill {{
"""
  yield from generate_cpp_enums(schema)
  yield f"""
}}  // namespace autofill

#endif  // {include_guard}"""

def generate_cpp_functions_header(schema, include_guard):
  yield f"""// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <limits>
#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {{
"""
  yield from generate_cpp_functions(schema)
  yield f"""
}}  // namespace autofill"""

if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description='Transpiles an Autofill AI schema from JSON to C++.')
  parser.add_argument(
      '--input',
      metavar='schema.json',
      required=True,
      type=str,
      help='JSON file containing the schema')
  parser.add_argument(
      '--output',
      metavar='out.h',
      required=True,
      type=str,
      nargs=2,
      help=(
          'C++ files to be generated: first the header for the'
          ' entity/attribute-type enums and second the source file for the'
          ' function implementations'
      ),
  )
  args = parser.parse_args()
  if not args.input or not args.output:
    parser.print_help()
    sys.exit(1)

  schema = parse_entity_schema(args.input)
  if not schema:
    sys.exit(1)

  def write_to_handle(generator, output_file):
    include_guard = re.sub(r'\W', '_', output_file.upper()) +'_'
    with io.open(output_file, 'w', encoding='utf-8') as output_handle:
      for line in generator(schema, include_guard):
        line += '\n'
        # unicode() exists and is necessary only in Python 2, not in Python 3.
        if sys.version_info[0] < 3:
          line = unicode(s, 'utf-8')
        output_handle.write(line)

  write_to_handle(generate_cpp_enums_header, args.output[0])
  write_to_handle(generate_cpp_functions_header, args.output[1])
