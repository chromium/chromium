#!/usr/bin/env python

# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
from collections import defaultdict
import io
import re
import sys
import json

# Generates a set of C++ constexpr constants to facilitate lookup of a set of
# MatchingPatterns by a given tuple (pattern name, language code).
#
# id_to_name_to_lang_to_patterns is a
# map from pattern source IDs to a
#   map from pattern names to a
#     map from language codes to
#       either a
#         list of patterns
#       or a
#         map of feature names to a
#           list of patterns
# where the pattern source IDs are consecutive natural numbers identifying the
# input JSON files.
# As a first step, any innermost maps of feature names to list of patterns are
# flattened to a list of patterns.
#
# The constants are:
#
# - kPatterns is an array of MatchingPatterns without duplicates.
#   The array is not sorted. The patterns are condensed (see below).
# - kPatterns__<pattern source id>__<pattern name>__<language code> contains
#   the indices of the pattterns that from the pattern source for that pattern
#   name and language code.
# - kPatternMap is a fixed flat map from (pattern name, language code) tuples
#   to arrays of spans of MatchPatternRefs; the indices of the array correspond
#   to the pattern sources.
# - kLanguages is a fixed flat set of language codes across all pattern source
#   ids and all pattern names.
#
# This representation has larger binary size on Android than using nested
# lookup pattern name -> language code -> span of MatchPatternRefs, but it's
# significantly simpler.
def generate_cpp_constants(id_to_name_to_lang_to_patterns):
  # Stores a `key` in `dictionary` and assigns it a natural number.
  #
  # For example, after memoize("foo", d) and memoize("bar", d),
  # d = {"foo": 0, "bar": 1}. This is useful to generate a C++ array
  # {"foo", "bar"} and referring to these elements by their indices
  # 0 and 1.
  def memoize(key, dictionary):
    if key not in dictionary:
      dictionary[key] = len(dictionary)
    return dictionary[key]

  # Maps a Python Boolean to a C++ Boolean literal.
  def python_bool_to_cpp(b):
    return 'true' if b else 'false'

  # Maps a string literal to a C++ string literal.
  def json_to_cpp_string_literal(json_string_literal):
    return json.dumps(json_string_literal or '')

  # Maps a string literal to a C++ UTF-16 string literal.
  def json_to_cpp_u16string_literal(json_string_literal):
    return 'u'+ json.dumps(json_string_literal or '')

  # Maps a list of strings to a DenseSet containing these values.
  # The strings represents constants in the format FOO_BAR.
  # They're translated to the C++ constant kFooBar.
  def json_to_cpp_dense_set(json_enum_values, cpp_enum_type):
    # Converts FOO_BAR into kFooBar.
    def json_to_cpp_constant_symbol(json):
      assert json.isupper()
      return f'{cpp_enum_type}::k' + re.sub(
          r'(^|_)([a-z])', lambda matched: matched.group(2).upper(),
          json.lower())
    cpp_enum_values = [json_to_cpp_constant_symbol(c) for c in json_enum_values]
    return f'DenseSet<{cpp_enum_type}>{{' + ','.join(cpp_enum_values) + f'}}'

  # Maps a list of strings to a DenseSet<MatchAttribute> expression.
  # The strings must be the names of MatchAttribute constants, e.g., NAME.
  # They're mapped to C++ constants, e.g., kName.
  def json_to_cpp_match_field_attributes(enum_values):
    return json_to_cpp_dense_set(enum_values, 'MatchAttribute')

  # Maps a list of strings to a DenseSet<FormControlType> expression.
  # The strings must be the names of FormControlType constants, e.g., TEXT_AREA.
  # They're mapped to C++ constants, e.g., kTextArea.
  def json_to_cpp_form_control_types(enum_values):
    return json_to_cpp_dense_set(enum_values, 'FormControlType')

  # Feature annotations are a tuple of (feature name, state). This function
  # maps them to the corresponding C++ OptionalRegexFeatureWithState.
  def feature_annotation_to_cpp_state(feature_annotation):
    if len(feature_annotation) == 0:
      return 'OptionalRegexFeatureWithState()'
    assert len(feature_annotation) == 2
    feature = "RegexFeature::k" + feature_annotation[0]
    enabled = python_bool_to_cpp(feature_annotation[1])
    return f'OptionalRegexFeatureWithState{{{feature}, {enabled}}}'

  # Maps a JSON object representing a pattern to a C++ MatchingPattern
  # expression.
  def json_to_cpp_pattern(json):
    positive_pattern = json_to_cpp_u16string_literal(json['positive_pattern'])
    negative_pattern = json_to_cpp_u16string_literal(json['negative_pattern'])
    # In the JSON files, every pattern is annotated with a 'positive_score'.
    # Since this is currently not used by the C++ logic, it is omitted to save
    # some binary size.
    match_field_attributes = json_to_cpp_match_field_attributes(
        json['match_field_attributes'])
    form_control_types = json_to_cpp_form_control_types(
        json['form_control_types'])
    feature = feature_annotation_to_cpp_state(json.get('feature', ()))
    return f'MatchingPattern{{\n' \
           f'  .positive_pattern = {positive_pattern},\n' \
           f'  .negative_pattern = {negative_pattern},\n' \
           f'  .match_field_attributes = {match_field_attributes},\n' \
           f'  .form_control_types = {form_control_types},\n' \
           f'  .feature = {feature},\n' \
           f'}}'

  # Name of the auxiliary C++ constant.
  def kPatterns(id, name, lang):
    return (f"kPatterns__{id}__{name}__{lang.replace('-', '_')}"
            if lang else f'kPatterns__{id}__{name}')

  # Preprocess the patterns.
  for id, name_to_lang_to_patterns in id_to_name_to_lang_to_patterns.items():
    if "__comment__" in name_to_lang_to_patterns:
      del name_to_lang_to_patterns["__comment__"]

    # Validate the JSON input.
    for name, lang_to_patterns in name_to_lang_to_patterns.items():
      if '' in lang_to_patterns:
        raise Exception('JSON format error: language is ""')

    # Flatten the feature flag layer by annotating patterns with a tuple of
    # their associated feature name and the desired feature state.
    for lang_to_patterns in name_to_lang_to_patterns.values():
      # Copy lang_to_patterns to modify the original map while iterating.
      for lang, patterns_or_map in lang_to_patterns.copy().items():
        if isinstance(patterns_or_map, list):
          continue
        assert isinstance(patterns_or_map, dict)
        feature_name = list(filter(len, patterns_or_map.keys()))
        # If feature annotations are used, there needs to be exactly one feature
        # name and at most one default arm.
        assert len(feature_name) == 1 and len(patterns_or_map) <= 2
        for feature, patterns in patterns_or_map.items():
          for pattern in patterns:
            pattern['feature'] = (feature_name[0], feature == feature_name[0])
        lang_to_patterns[lang] = [pattern for patterns in
          patterns_or_map.values() for pattern in patterns]

    # Remember each pattern's language.
    #
    # To ease debugging, we shall sort patterns of equal positive_score by their
    # language.
    for lang_to_patterns in name_to_lang_to_patterns.values():
      for lang, patterns in lang_to_patterns.items():
        for pattern in patterns:
          pattern['lang'] = lang

    # For each name, collect the items of all languages and add them as a
    # separate entry for the pseudo-language ''.
    for name, lang_to_patterns in name_to_lang_to_patterns.items():
      lang_to_patterns[''] = [
          p.copy() for ps in lang_to_patterns.values() for p in ps
      ]

    # Add the English patterns to all languages except for English itself and
    # the catch-all language ''.
    #
    # The idea is that these patterns should be applied (only) to the HTML
    # source code, i.e., not the user-visible labels. To this end, we mark the
    # English patterns here "supplementary", which will in the subsequent step
    # be encoded in the MatchPatternRef().
    for lang_to_patterns in name_to_lang_to_patterns.values():
      if 'en' not in lang_to_patterns:
        continue
      def make_supplementary_pattern(p):
        assert "NAME" in p['match_field_attributes']
        p = p.copy()
        p['supplementary'] = True
        return p
      for patterns in (patterns for lang, patterns in lang_to_patterns.items()
                       if lang not in ['', 'en']):
        patterns.extend(
            make_supplementary_pattern(p)
            for p in lang_to_patterns['en']
            if "NAME" in p['match_field_attributes'])

  # Populate the two maps:
  # - a map from C++ MatchingPattern expressions to their index.
  # - a map from names and languages and IDs to the their MatchingPatterns,
  #   represented as list of tuples (is_supplementary, pattern_index).
  pattern_to_index = {}
  name_to_lang_to_id_to_patternrefs = defaultdict(
      lambda: defaultdict(lambda: defaultdict(list)))
  for id, name_to_lang_to_patterns in id_to_name_to_lang_to_patterns.items():
    for name, lang_to_patterns in sorted(name_to_lang_to_patterns.items()):
      for lang, patterns in sorted(lang_to_patterns.items()):
        patternrefs = name_to_lang_to_id_to_patternrefs[name][lang][id]
        sort_key = lambda p: (-p['positive_score'], p['lang'])
        for pattern in sorted(patterns, key=sort_key):
          is_supplementary = ('supplementary' in pattern and
                              pattern['supplementary'])
          pattern_index = memoize(
              json_to_cpp_pattern(pattern), pattern_to_index)
          patternrefs.append((is_supplementary, pattern_index))

  # Generate the C++ constants.
  yield '// The patterns. Referred to by their index in MatchPatternRef.'
  yield 'constexpr auto kPatterns = std::to_array<MatchingPattern>({'
  for cpp_expr, index in sorted(
      pattern_to_index.items(), key=lambda item: item[1]):
    yield f'/*[{index}]=*/{cpp_expr},'
  yield '});'
  yield ''

  min_pattern_id = min(id_to_name_to_lang_to_patterns.keys())
  max_pattern_id = max(id_to_name_to_lang_to_patterns.keys())

  yield '// The patterns for field types and languages.'
  yield '// They are sorted by the patterns MatchingPattern::positive_score.'
  for name, lang_to_id_to_patternrefs in (
      name_to_lang_to_id_to_patternrefs.items()):
    for lang, id_to_patternrefs in lang_to_id_to_patternrefs.items():
      for id, patternrefs in id_to_patternrefs.items():
        # If another pattern source has the same patterns for this `name` and
        # `lang`, we don't need to a new array; a reference suffices.
        ids_with_the_same_patternrefs = [
            i for i in range(min_pattern_id, id)
            if i in name_to_lang_to_id_to_patternrefs[name][lang] and
            patternrefs == name_to_lang_to_id_to_patternrefs[name][lang][i]
        ]
        if ids_with_the_same_patternrefs != []:
          other_id = ids_with_the_same_patternrefs[0]
          yield (f'constexpr auto {kPatterns(id, name, lang)} = '
                 f'base::make_span({kPatterns(other_id, name, lang)});')
        else:
          yield (f'constexpr MatchPatternRef {kPatterns(id, name, lang)}[] {{' +
                 f', '.join(
                     f'MakeMatchPatternRef('+
                     f'{python_bool_to_cpp(is_supplementary)}, {index})'
                     for is_supplementary, index in patternrefs) + f'}};')
  yield ''

  yield '// The lookup map for field types and langs.'
  yield '//'
  yield '// The key type in the map is essentially a pair of const char*.'
  yield '// It also allows for lookup by std::string_view pairs (because the'
  yield '// comparator transparently accepts std::string_view pairs).'
  yield '//'
  yield '// The value type is an array of spans of MatchPatternRefs. The'
  yield '// indices of the array correspond to the pattern source: the patterns'
  yield '// from the first input JSON file are stored at index 0, etc.'
  yield '//'
  yield '// This design exploits that the different JSON files by and large'
  yield '// contain the same pattern names and languages. If instead we'
  yield '// generated an individual map for each JSON file, then, assuming four'
  yield '// JSON files, the duplicate keys would cause 60% overhead, which'
  yield '// adds up to >10K binary size on Android.'
  yield 'constexpr auto kPatternMap = base::MakeFixedFlatMap<' \
      'NameAndLanguage, '\
      'std::array<base::span<const MatchPatternRef>, ' \
      f'{max_pattern_id - min_pattern_id + 1}' \
      '>>({'
  for name, lang_to_id_to_patternrefs in sorted(
      name_to_lang_to_id_to_patternrefs.items()):
    for lang, id_to_patternrefs in sorted(lang_to_id_to_patternrefs.items()):
      pattern_array = [
          kPatterns(id, name, lang)
          if id in id_to_patternrefs else 'base::span<const MatchPatternRef>{}'
          for id in range(min_pattern_id, max_pattern_id + 1)
      ]
      yield f'  {{{{"{name}", "{lang}"}}, {{{", ".join(pattern_array)}}}}},'
  yield '}, NameAndLanguageComparator());'
  yield ''

  language_array = sorted(
      set(f'"{lang}"' for lang_to_id_to_patternrefs in
          name_to_lang_to_id_to_patternrefs.values()
          for lang in lang_to_id_to_patternrefs.keys() if lang != ''))
  yield '// The set of language codes across all language source ids and'
  yield '// pattern names.'
  yield 'constexpr auto kLanguages = base::MakeFixedFlatSet<const char*>({'
  yield f'  {", ".join(language_array)}'
  yield '}, LanguageComparator());'

def generate_cpp_lines(id_to_name_to_lang_to_patterns):
  yield """// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_REGEX_PATTERNS_INL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_REGEX_PATTERNS_INL_H_

#include <array>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"

#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"

namespace autofill {

// Wrapper of MatchPatternRef's private constructor.
// It's a friend of MatchPatternRef.
constexpr MatchPatternRef MakeMatchPatternRef(
    bool is_supplementary,
    MatchPatternRef::UnderlyingType index) {
  return MatchPatternRef(is_supplementary, index);
}

// A pair of const char* used as keys in the `kPatternMap`.
struct NameAndLanguage {
  using StringViewPair = std::pair<std::string_view, std::string_view>;

  // By this implicit conversion, the below comparator can be called for
  // NameAndLanguageComparator and StringViewPairs alike.
  constexpr operator StringViewPair() const {
    return {std::string_view(name), std::string_view(lang)};
  }

  const char* name;  // A pattern name.
  const char* lang;  // A language code.
};

// A less-than relation on NameAndLanguage and/or std::string_view pairs.
struct NameAndLanguageComparator {
  using is_transparent = void;

  // By way of the implicit conversion from NameAndLanguage to StringViewPair,
  // this function also accepts NameAndLanguage.
  //
  // To implement constexpr lexicographic comparison of const char* with the
  // standard library, we need to compute both the lengths of the strings before
  // we can actually compare the strings. A simple way of doing so is to convert
  // each const char* to a std::string_view and then comparing the
  // std::string_views.
  //
  // This is exactly what the comparator does: when an argument is a
  // NameAndLanguage, it is implicitly converted to a StringViewPair, which
  // is then compared to the other StringViewPair.
  constexpr bool operator()(
      NameAndLanguage::StringViewPair a,
      NameAndLanguage::StringViewPair b) const {
    int cmp = a.first.compare(b.first);
    return cmp < 0 || (cmp == 0 && a.second.compare(b.second) < 0);
  }
};

// A less-than relation on const char* and std::string_view, in particular for
// language codes.
struct LanguageComparator {
  using is_transparent = void;

  // This function also accepts const char* by implicit conversion to
  // std::string_view.
  //
  // This comparator facilitates constexpr comparison among const char*
  // similarly to the above NameAndLanguageComparator.
  constexpr bool operator()(std::string_view a, std::string_view b) const {
    return a.compare(b) < 0;
  }
};
"""
  yield from generate_cpp_constants(id_to_name_to_lang_to_patterns)
  yield """
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_REGEX_PATTERNS_INL_H_
"""

def build_cpp_file(id_to_name_to_lang_to_patterns, output_handle):
  for line in generate_cpp_lines(id_to_name_to_lang_to_patterns):
    line += '\n'
    # unicode() exists and is necessary only in Python 2, not in Python 3.
    if sys.version_info[0] < 3:
      line = unicode(s, 'utf-8')
    output_handle.write(line)

def parse_json(input_files, output_file):
  id_to_name_to_lang_to_patterns = {}
  for index, input_file in enumerate(input_files):
    with io.open(input_file, 'r', encoding='utf-8') as input_handle:
      id_to_name_to_lang_to_patterns[index] = json.load(input_handle)

  with io.open(output_file, 'w', encoding='utf-8') as output_handle:
    build_cpp_file(id_to_name_to_lang_to_patterns, output_handle)

if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description='Transpiles parsing patterns from JSON to C++.')
  parser.add_argument(
      '--input',
      metavar='json-file',
      required=True,
      type=str,
      nargs='+',
      help='JSON file(s) containing patterns')
  parser.add_argument(
      '--output',
      metavar='header-file',
      required=True,
      type=str,
      help='C++ header file to be generated')
  args = parser.parse_args()
  if not args.input or not args.output:
    parser.print_help()
  else:
    parse_json(args.input, args.output)
