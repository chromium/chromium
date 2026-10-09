# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for //chrome/test.

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts/
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

# BUILD.gn files that are edited by many concurrent CLs. The global
# CheckPatchFormatted() in the root PRESUBMIT.py only emits a warning, which is
# easy to miss; when unformatted edits to these large, high-traffic files land
# close together they cause `gn format` churn and needless merge conflicts.
# Enforce `git cl format` as an error on these specific files only, to keep the
# blast radius small.
_FORMAT_REQUIRED_FILES = ('chrome/test/BUILD.gn',)


def CheckHighTrafficBuildGnFormatted(input_api, output_api):
  return input_api.canned_checks.CheckPatchFormatted(
    input_api,
    output_api,
    result_factory=output_api.PresubmitError,
    file_filter=lambda f: f.LocalPath() in _FORMAT_REQUIRED_FILES,
  )


_TEST_BUILD_GN = 'chrome/test/BUILD.gn'
_TEST_DIR = 'chrome/test'

# Matches a quoted test source file, e.g. "../browser/foo/foo_unittest.cc" or
# "//chrome/browser/foo/foo_browsertest.cc". Paths built from GN variables
# (e.g. "$root_gen_dir/...") are ignored.
_TEST_SOURCE_RE = r'"([^"$]*test\.(?:cc|mm))"'


def _StripComment(line):
  """Returns `line` without its GN comment, if any.

  A "#" starts a comment unless it is inside a string, so both
  `# "foo_unittest.cc"` and `"foo.cc",  # Was "foo_unittest.cc".` keep only the
  code before the comment.
  """
  in_string = False
  escaped = False
  for i, c in enumerate(line):
    if escaped:
      escaped = False
    elif c == '\\':
      escaped = True
    elif c == '"':
      in_string = not in_string
    elif c == '#' and not in_string:
      return line[:i]
  return line


def _GetTestSources(input_api, lines):
  """Returns the set of test source files listed in `lines`.

  Paths are normalized to be relative to the source root, e.g.
  "../browser/foo/foo_unittest.cc" and "//chrome/browser/foo/foo_unittest.cc"
  both become "chrome/browser/foo/foo_unittest.cc".
  """
  # GN paths always use forward slashes, regardless of the host OS.
  import posixpath

  sources = set()
  for line in lines:
    line = _StripComment(line)
    for path in input_api.re.findall(_TEST_SOURCE_RE, line):
      if path.startswith('//'):
        path = path[2:]
      else:
        path = posixpath.join(_TEST_DIR, path)
      sources.add(posixpath.normpath(path))
  return sources


def CheckNoNewTestSources(input_api, output_api):
  """Warns when test files outside //chrome/test are added to its BUILD.gn.

  New tests should go in a test target in the BUILD.gn of the directory that
  contains them (for example a "unit_tests" or "browser_tests" source_set),
  which chrome/test/BUILD.gn then depends on. See https://crbug.com/521960574.
  """
  new_sources = set()
  for f in input_api.AffectedFiles(include_deletes=False):
    if f.LocalPath().replace('\\', '/') != _TEST_BUILD_GN:
      continue
    old = _GetTestSources(input_api, f.OldContents())
    new = _GetTestSources(input_api, f.NewContents())
    new_sources |= new - old

  new_sources = sorted(
    s for s in new_sources if not s.startswith(_TEST_DIR + '/')
  )
  if not new_sources:
    return []
  return [
    output_api.PresubmitPromptWarning(
      f'Please don\'t add new test files to {_TEST_BUILD_GN}. Instead, add '
      'them to a test target (e.g. a "unit_tests" or "browser_tests" '
      'source_set) in the BUILD.gn file of the directory containing the '
      f'test, and depend on that target from {_TEST_BUILD_GN}. See '
      'https://crbug.com/521960574.',
      items=new_sources,
    )
  ]
