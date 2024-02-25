# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

INCLUDE_CPP_FILES_ONLY = (
  r'.*\.(cc|h)$',
)

def CheckChangeLintsClean(input_api, output_api):
  """Makes sure that the change is cpplint clean."""
  # The only valid extensions for cpplint are .cc, .h, .cpp, .cu, and .ch.
  sources = lambda x: input_api.FilterSourceFile(
    x, files_to_check=INCLUDE_CPP_FILES_ONLY,
    files_to_skip=input_api.DEFAULT_FILES_TO_SKIP)
  # lint_filters=[] stops the OFF_BY_DEFAULT_LINT_FILTERS from being disabled,
  # finding many more issues. verbose_level=1 finds a small number of additional
  # issues.
  return input_api.canned_checks.CheckChangeLintsClean(
      input_api, output_api, sources, lint_filters=[], verbose_level=1)


def CheckChange(input_api, output_api):
  results = []
  results += CheckChangeLintsClean(input_api, output_api)
  return results

def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)

