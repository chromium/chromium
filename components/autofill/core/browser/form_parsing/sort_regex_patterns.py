#!/usr/bin/env python

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Sorts all patterns in a given JSON file alphabetically by name, and for each
# pattern, sorts all of its languages alphabetically. It is ensured that the
# top-level "__comment__" and "PATTERN_SOURCE_DUMMY" patterns remain on top.
#
# Example usage (from inside components/autofill/core/browser/form_parsing/):
# ./sort_regex_patterns.py resources/legacy_regex_patterns.json
#   | sponge resources/legacy_regex_patterns.json

import argparse
import json

# Sorts the `pattern_json` and returns a pretty-printed, sorted JSON string.
def sort_patterns(pattern_json):
  # The output JSON (dicts are ordered by insertion time). "__comment__"
  # receives special treatment because its value is not a dict.
  output = { "__comment__" : pattern_json["__comment__"] }
  # Put "PATTERN_SOURCE_DUMMY" in front and sort the rest.
  for pattern_name in ["PATTERN_SOURCE_DUMMY"] + sorted(
      key for key in pattern_json.keys()
      if key not in ["__comment__", "PATTERN_SOURCE_DUMMY"]):
    # Order by language.
    output[pattern_name] = dict(sorted(pattern_json[pattern_name].items()))
  return json.dumps(output, indent=2, ensure_ascii=False)

if __name__ == '__main__':
  parser = argparse.ArgumentParser(description="Sorts a JSON file of patterns")
  parser.add_argument("file", type=str, help="JSON pattern file name + path")
  args = parser.parse_args()
  with open(args.file, mode='r', encoding="utf-8") as handle:
    print(sort_patterns(json.load(handle)))
