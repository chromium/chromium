#!/usr/bin/env python

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import json
import sys

def to_string_literal(json_string_literal):
  return json.dumps(json_string_literal)

def build_cpp_map_population(input):
  lines = []
  def output(line):
    lines.append(line)

  output('constexpr JsonPattern patterns[] = {')
  for key1 in input:
    for key2 in input[key1]:
      for pattern in input[key1][key2]:
        name = to_string_literal(key1)
        language = to_string_literal(key2)
        positive_pattern = pattern['positive_pattern']
        negative_pattern = pattern['negative_pattern']
        positive_score = pattern['positive_score']
        match_field_attributes = pattern['match_field_attributes']
        match_field_input_types = pattern['match_field_input_types']

        positive_pattern = 'u' + to_string_literal(positive_pattern)

        if negative_pattern is None:
          negative_pattern = 'nullptr';
        else:
          negative_pattern = 'u' + to_string_literal(negative_pattern)

        match_field_attributes = map(
                lambda i: 'To<MatchAttribute, {}>()'.format(i),
                match_field_attributes)
        match_field_input_types = map(
                lambda i: 'To<MatchFieldType, {}>()'.format(i),
                match_field_input_types)

        match_field_attributes = '{{ {} }}'.format(
                ','.join(match_field_attributes))
        match_field_input_types = '{{ {} }}'.format(
                ','.join(match_field_input_types))

        output('{')
        output('.name = {},'.format(name))
        output('.language = {},'.format(language))
        output('.positive_pattern = {},'.format(positive_pattern))
        output('.negative_pattern = {},'.format(negative_pattern))
        output('.positive_score = {},'.format(positive_score))
        output('.match_field_attributes = {},'.format(match_field_attributes))
        output('.match_field_input_types = {},'.format(match_field_input_types))
        output('},')

  output('};')

  return lines


def build_cpp_function(cpp, output_handle):
  def output(s):
    # unicode() exists and is necessary only in Python 2, not in Python 3.
    if sys.version_info[0] < 3:
      s = unicode(s, 'utf-8')
    output_handle.write(s)

  output('// Copyright 2020 The Chromium Authors. All rights reserved.\n')
  output('// Use of this source code is governed by a BSD-style license ')
  output('that can be\n')
  output('// found in the LICENSE file.\n')
  output('\n')
  output('#include "components/autofill/core/browser/pattern_provider/'\
               'default_regex_patterns.h"\n')
  output('#include "components/autofill/core/common/language_code.h"\n')
  output('\n')
  output('namespace autofill {\n')
  output('\n')
  output('namespace {\n')
  output('\n')
  output('template<typename Enum, int i>\n')
  output('constexpr Enum To() {\n')
  output('  static_assert(0 <= i);\n')
  output('  static_assert(static_cast<Enum>(i) <= Enum::kMaxValue);\n')
  output('  return static_cast<Enum>(i);\n')
  output('}\n')
  output('\n')
  output('}  // namespace\n')
  output('\n')
  output('PatternProvider::Map CreateDefaultRegexPatterns() {\n')
  output('  struct JsonPattern {\n')
  output('    const char* name;\n')
  output('    const char* language;\n')
  output('    const char16_t* positive_pattern;\n')
  output('    const char16_t* negative_pattern;\n')
  output('    float positive_score;\n')
  output('    DenseSet<MatchAttribute> match_field_attributes;\n')
  output('    DenseSet<MatchFieldType> match_field_input_types;\n')
  output('  };\n')
  output('\n')
  for line in build_cpp_map_population(cpp):
    output(line)
    output('\n')
  output('  PatternProvider::Map map;\n')
  output('  size_t len = sizeof(patterns) / sizeof(patterns[0]);\n')
  output('  for (size_t i = 0; i < len; ++i) {\n')
  output('    const JsonPattern& p = patterns[i];\n')
  output('    MatchingPattern mp;\n')
  output('    mp.language = LanguageCode(p.language);\n')
  output('    mp.positive_pattern = p.positive_pattern;\n')
  output('    mp.negative_pattern = '
               'p.negative_pattern ? p.negative_pattern : u"";\n')
  output('    mp.positive_score = p.positive_score;\n')
  output('    mp.match_field_input_types = p.match_field_input_types;\n')
  output('    mp.match_field_attributes = p.match_field_attributes;\n')
  output('    map[p.name][LanguageCode(p.language)].push_back(mp);\n')
  output('  }\n')
  output('  return map;\n')
  output('}\n')
  output('\n')
  output('}')

if __name__ == '__main__':
  input_file = sys.argv[1]
  output_file = sys.argv[2]
  with io.open(input_file, 'r', encoding='utf-8') as input_handle:
    input_json = json.load(input_handle)
    with io.open(output_file, 'w', encoding='utf-8') as output_handle:
      build_cpp_function(input_json, output_handle)
