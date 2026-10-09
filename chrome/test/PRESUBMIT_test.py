#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import PRESUBMIT

sys.path.insert(
  0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
)
from PRESUBMIT_test_mocks import MockAffectedFile
from PRESUBMIT_test_mocks import MockInputApi, MockOutputApi

_OLD_BUILD_GN = [
  'test("unit_tests") {',
  '  sources = [',
  '    "../browser/existing/existing_unittest.cc",',
  '    "//chrome/common/existing/existing_unittest.cc",',
  '    "base/existing_test_util.cc",',
  '  ]',
  '}',
]


class CheckNoNewTestSourcesTest(unittest.TestCase):
  def _Check(self, new_lines, path='chrome/test/BUILD.gn', action='M'):
    """Runs the check on a change that inserts `new_lines` into the test
    target in _OLD_BUILD_GN (or adds them as a new file for action 'A')."""
    if action == 'A':
      old, new = [], new_lines
    else:
      old = _OLD_BUILD_GN
      new = old[:2] + new_lines + old[2:]
    input_api = MockInputApi()
    input_api.files = [MockAffectedFile(path, new, old, action=action)]
    return PRESUBMIT.CheckNoNewTestSources(input_api, MockOutputApi())

  def _AssertWarns(self, results, items):
    self.assertEqual(1, len(results))
    self.assertEqual('warning', results[0].type)
    self.assertEqual(items, results[0].items)

  def testNewRelativeTestPathWarns(self):
    results = self._Check(['    "../browser/foo/foo_unittest.cc",'])
    self._AssertWarns(results, ['chrome/browser/foo/foo_unittest.cc'])

  def testNewAbsoluteTestPathWarns(self):
    results = self._Check(['    "//chrome/common/bar/bar_browsertest.cc",'])
    self._AssertWarns(results, ['chrome/common/bar/bar_browsertest.cc'])

  def testNewPathOutsideChromeWarns(self):
    results = self._Check(
      [
        '    "../../components/foo/foo_unittest.cc",',
        '    "//content/bar/bar_browsertest.cc",',
      ]
    )
    self._AssertWarns(
      results,
      [
        'components/foo/foo_unittest.cc',
        'content/bar/bar_browsertest.cc',
      ],
    )

  def testVariousTestSuffixesWarn(self):
    results = self._Check(
      [
        '    "../browser/a/a_test.cc",',
        '    "../browser/b/b_uitest.cc",',
        '    "../browser/c/c_interactive_uitest.cc",',
        '    "../browser/d/d_browsertest.mm",',
      ]
    )
    self._AssertWarns(
      results,
      [
        'chrome/browser/a/a_test.cc',
        'chrome/browser/b/b_uitest.cc',
        'chrome/browser/c/c_interactive_uitest.cc',
        'chrome/browser/d/d_browsertest.mm',
      ],
    )

  def testSingleLineListWarns(self):
    results = self._Check(['  sources += [ "../browser/foo/foo_unittest.cc" ]'])
    self._AssertWarns(results, ['chrome/browser/foo/foo_unittest.cc'])

  def testMultipleNewFilesAreSortedInOneWarning(self):
    results = self._Check(
      [
        '    "//chrome/browser/z/z_unittest.cc",',
        '    "../browser/a/a_unittest.cc",',
      ]
    )
    self._AssertWarns(
      results,
      [
        'chrome/browser/a/a_unittest.cc',
        'chrome/browser/z/z_unittest.cc',
      ],
    )

  def testNewFileInChromeTestRelativeIsAllowed(self):
    self.assertEqual(
      [],
      self._Check(
        [
          '    "foo/bar/baz_ui_test.cc",',
          '    "./foo/qux_unittest.cc",',
          '    "../test/foo/other_browsertest.cc",',
        ]
      ),
    )

  def testNewFileInChromeTestAbsoluteIsAllowed(self):
    self.assertEqual(
      [], self._Check(['    "//chrome/test/foo/bar/baz_ui_test.cc",'])
    )

  def testSimilarlyNamedDirectoryIsNotChromeTest(self):
    results = self._Check(['    "//chrome/test_foo/foo_unittest.cc",'])
    self._AssertWarns(results, ['chrome/test_foo/foo_unittest.cc'])

  def testNonTestFileIsAllowed(self):
    self.assertEqual(
      [],
      self._Check(
        [
          '    "../browser/foo/foo_test_util.cc",',
          '    "../browser/foo/foo_test_utils.h",',
          '    "../browser/foo/fake_foo.cc",',
          '    "../browser/foo/foo_unittest.h",',
        ]
      ),
    )

  def testGnVariablePathIsIgnored(self):
    self.assertEqual(
      [], self._Check(['    "$root_gen_dir/chrome/foo/foo_unittest.cc",'])
    )

  def testCommentIsIgnored(self):
    self.assertEqual(
      [], self._Check(['    # "../browser/foo/foo_unittest.cc" was moved.'])
    )

  def testTrailingCommentIsIgnored(self):
    # The path before the comment is still checked.
    results = self._Check(
      ['    "../browser/foo/foo_unittest.cc",  # Comment here']
    )
    self._AssertWarns(results, ['chrome/browser/foo/foo_unittest.cc'])
    # Paths inside the comment are ignored.
    self.assertEqual(
      [],
      self._Check(
        ['    "foo/bar.cc",  # Moved from "../browser/foo/foo_unittest.cc".']
      ),
    )

  def testHashInsideStringIsNotAComment(self):
    results = self._Check(['    "../browser/foo#bar/foo_unittest.cc",'])
    self._AssertWarns(results, ['chrome/browser/foo#bar/foo_unittest.cc'])

  def testExistingFileIsAllowed(self):
    # Unchanged lines, e.g. diff context, don't warn.
    self.assertEqual([], self._Check([]))

  def testMovingExistingFileWithinBuildGnIsAllowed(self):
    old = _OLD_BUILD_GN + [
      'test("browser_tests") {',
      '  sources = []',
      '}',
    ]
    new = [line for line in _OLD_BUILD_GN if 'browser/existing' not in line] + [
      'test("browser_tests") {',
      '  sources = [ "../browser/existing/existing_unittest.cc" ]',
      '}',
    ]
    input_api = MockInputApi()
    input_api.files = [MockAffectedFile('chrome/test/BUILD.gn', new, old)]
    self.assertEqual(
      [], PRESUBMIT.CheckNoNewTestSources(input_api, MockOutputApi())
    )

  def testSwitchingBetweenRelativeAndAbsolutePathIsAllowed(self):
    old = _OLD_BUILD_GN
    new = [
      line.replace(
        '"../browser/existing/', '"//chrome/browser/existing/'
      ).replace('"//chrome/common/existing/', '"../common/existing/')
      for line in old
    ]
    self.assertNotEqual(old, new)
    input_api = MockInputApi()
    input_api.files = [MockAffectedFile('chrome/test/BUILD.gn', new, old)]
    self.assertEqual(
      [], PRESUBMIT.CheckNoNewTestSources(input_api, MockOutputApi())
    )

  def testOtherBuildGnFileIsIgnored(self):
    self.assertEqual(
      [],
      self._Check(
        ['    "../browser/foo/foo_unittest.cc",'],
        path='chrome/test/foo/BUILD.gn',
      ),
    )
    self.assertEqual(
      [],
      self._Check(
        ['    "foo_unittest.cc",'], path='chrome/browser/foo/BUILD.gn'
      ),
    )

  def testWindowsPathSeparatorsAreHandled(self):
    results = self._Check(
      ['    "../browser/foo/foo_unittest.cc",'], path='chrome\\test\\BUILD.gn'
    )
    self._AssertWarns(results, ['chrome/browser/foo/foo_unittest.cc'])

  def testDeletedFileIsIgnored(self):
    input_api = MockInputApi()
    input_api.files = [
      MockAffectedFile('chrome/test/BUILD.gn', [], _OLD_BUILD_GN, action='D')
    ]
    self.assertEqual(
      [], PRESUBMIT.CheckNoNewTestSources(input_api, MockOutputApi())
    )


if __name__ == '__main__':
  unittest.main()
