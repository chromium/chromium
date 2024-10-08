# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest
import os
import difflib
import sys
import glob
import tempfile
import pathlib
from typing import List, Set

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir))
sys.path.insert(0, REPOSITORY_ROOT)

import generate_build_scripts_output as build_scripts_generator  # pylint: disable=wrong-import-position

_SCRIPT_DIR = os.path.normpath(os.path.dirname(__file__))
_GOLDENS_DIR = os.path.join(_SCRIPT_DIR, 'golden')
# Set this environment variable in order to regenerate the golden text
# files.
_REBASELINE = os.environ.get('REBASELINE', '0') != '0'

_accessed_goldens = set()


class TestSum(unittest.TestCase):

    def _read_golden_file(self, path):
        _accessed_goldens.add(path)
        if not os.path.exists(path):
            return None
        with open(path, 'r') as f:
            return f.read()

    def assert_golden_test_equals(self, generated_text, golden_file):
        """Compares generated text with the corresponding golden_file

      It will instead compare the generated text with
      script_dir/golden/golden_file."""
        golden_path = os.path.join(_GOLDENS_DIR, golden_file)
        golden_text = self._read_golden_file(golden_path)
        if _REBASELINE:
            if golden_text != generated_text:
                print('Updated', golden_path)
                with open(golden_path, 'w') as f:
                    f.write(generated_text)
            return
        # golden_text is None if no file is found. Better to fail than in
        # AssertTextEquals so we can give a clearer message.
        if golden_text is None:
            self.fail('Golden file does not exist: ' + golden_path)
        self.assert_text_equals(golden_text, generated_text)

    def assert_text_equals(self, golden_text, generated_text):
        if not self.compare_text(golden_text, generated_text):
            self.fail('Golden text mismatch.')

    def compare_text(self, golden_text, generated_text):

        def filter_text(text):
            return [
                l.strip() for l in text.split('\n')
                if not l.startswith('// Copyright')
            ]

        stripped_golden = filter_text(golden_text)
        stripped_generated = filter_text(generated_text)
        if stripped_golden == stripped_generated:
            return True
        print(self.id())
        for line in difflib.context_diff(stripped_golden, stripped_generated):
            print(line)
        print('\n\nGenerated')
        print('=' * 80)
        print(generated_text)
        print('=' * 80)
        print('Run with:')
        print('REBASELINE=1', sys.argv[0])
        print('to regenerate the data files.')

    def _test_end_to_end_generation(self,
                                    archs: List[str],
                                    targets: List[str] = None):

        temp_file = tempfile.NamedTemporaryFile(delete=False)
        # We dont collect transitive dependencies build_output in the
        # tests because it will start collecting std build_output which
        # can change often on new releases of rust.
        build_scripts_generator.dump_build_scripts_outputs_to_file(
            temp_file.name, archs, targets)
        self.assert_golden_test_equals(
            pathlib.Path(temp_file.name).read_text(),
            f"{self._testMethodName}.golden")
        temp_file.close()
        os.unlink(temp_file.name)

    def test_simple_rust_library_x86(self):
        self._test_end_to_end_generation(["x86"], [
            "//components/cronet/gn2bp/tests/test_rlib_crate:target1_gn2bp_test"
        ])

    def test_simple_rust_library_all_archs(self):
        self._test_end_to_end_generation([
            "x86", "x64", "arm", "arm64", "riscv64"
        ], [
            "//components/cronet/gn2bp/tests/test_rlib_crate:target1_gn2bp_test",
            "//components/cronet/gn2bp/tests/test_rlib_crate:target2_gn2bp_test"
        ])


def main():
    try:
        unittest.main()
    finally:
        if _REBASELINE and not any(not x.startswith('-')
                                   for x in sys.argv[1:]):
            for path in glob.glob(os.path.join(_GOLDENS_DIR, '*.golden')):
                if path not in _accessed_goldens:
                    print('Removing obsolete golden:', path)
                    os.unlink(path)


if __name__ == '__main__':
    main()
