# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import json
import os
import re
import sys

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.insert(0, REPOSITORY_ROOT)

import components.cronet.tools.breakages_constants as breakages_constants  # pylint: disable=wrong-import-position
import components.cronet.tools.utils as cronet_utils  # pylint: disable=wrong-import-position

FILE_PATH = os.path.join(REPOSITORY_ROOT, 'components', 'cronet', 'android',
                         'breakages.json')


class TestBreakagesJson(unittest.TestCase):

  parsed_breakages = []

  @classmethod
  def createFromBreakagesJson(cls):
    """Loads the JSON file once before tests run."""
    if not os.path.exists(FILE_PATH):
      raise FileNotFoundError(
          f"CRITICAL: The file '{FILE_PATH}' could not be found.")

    content = json.loads(cronet_utils.read_file(FILE_PATH))
    if "breakages" not in content:
      raise ValueError("CRITICAL: Root of JSON must contain a 'breakages' key.")

    if not isinstance(content["breakages"], list):
      raise ValueError("CRITICAL: 'breakages' key must contain a list.")

    cls.parsed_breakages = content["breakages"]

  def _is_valid_change_id(self, change_id):
    if not isinstance(change_id, str):
      return False
    pattern = r'^I[0-9a-fA-F]{40}$'
    zeros_pattern = r'^I00*$'
    return bool(
        re.fullmatch(pattern, change_id)
        and not re.fullmatch(zeros_pattern, change_id))

  def _format_error(self, entry, message):
    entry_str = json.dumps(entry, indent=2)
    return f"\n{message}\n\nFailing Entry:\n{entry_str}\n"

  def test_id_validation_logic(self):
    self.assertTrue(self._is_valid_change_id("I" + "a" * 40))
    self.assertFalse(self._is_valid_change_id("I" + "a" * 39))  # Too short
    self.assertFalse(self._is_valid_change_id("a" * 40))  # Missing 'I'
    self.assertFalse(self._is_valid_change_id("I" + "z" * 40))  # Non-hex
    self.assertFalse(self._is_valid_change_id("I" + "0" * 40))  # All zeros

  def test_missing_mandatory_keys(self):
    required_keys = [
        breakages_constants.BUG_TXT, breakages_constants.BAD_CHANGE_ID_TXT,
        breakages_constants.GOOD_CHANGE_IDS_TXT, breakages_constants.COMMENT_TXT
    ]

    for entry in self.parsed_breakages:
      for key in required_keys:
        self.assertIn(
            key, entry,
            self._format_error(entry, f"Missing mandatory key: '{key}'"))

  def test_unknown_keys(self):
    allowed_keys = {
        breakages_constants.GOOD_CHANGE_IDS_TXT,
        breakages_constants.BAD_CHANGE_ID_TXT, breakages_constants.BUG_TXT,
        breakages_constants.COMMENT_TXT
    }

    for entry in self.parsed_breakages:
      for key in entry:
        if key.startswith('_'):
          continue  # comments are allowed
        self.assertIn(
            key, allowed_keys,
            self._format_error(
                entry, f"Unknown key found: '{key}'. Allowed: {allowed_keys}"))

  def test_bad_change_id_format_in_file(self):
    """Test that 'bad_change_id' entries in the file follow the regex."""
    for entry in self.parsed_breakages:
      val = entry[breakages_constants.BAD_CHANGE_ID_TXT]
      self.assertTrue(
          self._is_valid_change_id(val),
          self._format_error(
              entry,
              f"Invalid '{breakages_constants.BAD_CHANGE_ID_TXT}' format: '{val}'"
          ))

  def test_good_change_ids_is_list(self):
    for entry in self.parsed_breakages:
      val = entry[breakages_constants.GOOD_CHANGE_IDS_TXT]
      self.assertIsInstance(
          val, list,
          self._format_error(
              entry,
              f"'{breakages_constants.GOOD_CHANGE_IDS_TXT}' must be a list. Found {type(val)}"
          ))

  def test_good_change_ids_format_in_file(self):
    for entry in self.parsed_breakages:
      for val in entry[breakages_constants.GOOD_CHANGE_IDS_TXT]:
        self.assertTrue(
            self._is_valid_change_id(val),
            self._format_error(
                entry,
                f"Invalid ID in '{breakages_constants.GOOD_CHANGE_IDS_TXT}': '{val}'"
            ))


if __name__ == '__main__':
  unittest.main()
