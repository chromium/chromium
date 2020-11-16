#!/usr/bin/env python

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import json
import sys

def to_string_literal(json_string_literal):
  return json.dumps(json_string_literal)

def build_cpp_map_population(input):
  lines = []
  def output(line):
    lines.append(line)

  output('JsonPattern patterns[] = {')
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

        positive_pattern = to_string_literal(positive_pattern)

        if negative_pattern is None:
          negative_pattern = 'nullptr';
        else:
          negative_pattern = to_string_literal(negative_pattern)

        # Shift to the right to match the MatchFieldTypes enum, which
        # temporarily starts at 1<<2 instead of 1<<0.
        match_field_input_types = '{} << 2'.format(match_field_input_types)

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


def build_cpp_function(cpp, output):
  output.write('// Copyright 2020 The Chromium Authors. All rights reserved.\n')
  output.write('// Use of this source code is governed by a BSD-style license ')
  output.write('that can be\n')
  output.write('// found in the LICENSE file.\n')
  output.write('\n')
  output.write('#include "components/autofill/core/browser/pattern_provider/'\
               'default_regex_patterns.h"\n')
  output.write('#include "components/autofill/core/common/language_code.h"\n')
  output.write('\n')
  output.write('namespace autofill {\n')
  output.write('\n')
  output.write('PatternProvider::Map CreateDefaultRegexPatterns() {\n')
  output.write('  struct JsonPattern {\n')
  output.write('    const char* name;\n')
  output.write('    const char* language;\n')
  output.write('    const char* positive_pattern;\n')
  output.write('    const char* negative_pattern;\n')
  output.write('    float positive_score;\n')
  output.write('    uint8_t match_field_attributes;\n')
  output.write('    uint16_t match_field_input_types;\n')
  output.write('  };\n')
  output.write('\n')
  for line in build_cpp_map_population(cpp):
    output.write(line)
    output.write('\n')
  output.write('  PatternProvider::Map map;\n')
  output.write('  size_t len = sizeof(patterns) / sizeof(patterns[0]);\n')
  output.write('  for (size_t i = 0; i < len; ++i) {\n')
  output.write('    const JsonPattern& p = patterns[i];\n')
  output.write('    MatchingPattern mp;\n')
  output.write('    mp.language = LanguageCode(p.language);\n')
  output.write('    mp.positive_pattern = p.positive_pattern;\n')
  output.write('    mp.negative_pattern = '
               'p.negative_pattern ? p.negative_pattern : "";\n')
  output.write('    mp.positive_score = p.positive_score;\n')
  output.write('    mp.match_field_input_types = p.match_field_input_types;\n')
  output.write('    mp.match_field_attributes = p.match_field_attributes;\n')
  output.write('    map[p.name][LanguageCode(p.language)].push_back(mp);\n')
  output.write('  }\n')
  output.write('  return map;\n')
  output.write('}\n')
  output.write('\n')
  output.write('}')

if __name__ == '__main__':
  input_file = sys.argv[1]
  output_file = sys.argv[2]
  with open(input_file, 'r') as handle:
    input = json.loads(handle.read())
    with open(output_file, 'w') as output:
      build_cpp_function(input, output)
