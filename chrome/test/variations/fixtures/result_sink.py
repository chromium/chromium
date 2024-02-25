# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import logging
import os
import sys

from typing import Any, Callable, List, Tuple, Mapping

import pytest

from chrome.test.variations.test_utils import SRC_DIR

# The module result_sink is under build/util and imported relative to its root.
sys.path.append(os.path.join(SRC_DIR, 'build', 'util'))

from lib.results import result_sink
from lib.results import result_types

"""Associates an artifact with the current test.

Args:
  artifact_name: The artifact name
  file_path: The path to the file to be uploaded.
"""
AddArtifact = Callable[[str, str], None]

"""Associates a tag with the current test.

Args:
  tag_key: The tag key
  tag_value: The string value of the tag.
"""
AddTag = Callable[[str, str], None]

_RESULT_TYPES = {
  'passed': result_types.PASS,
  'failed': result_types.FAIL,
  'skipped': result_types.SKIP,
}

_PROPERTY_SECTION_TAG = 'tag'
_PROPERTY_SECTION_ARTIFACT = 'artifact'


def pytest_sessionstart(session: pytest.Session):
  session.sink_client = result_sink.TryInitClient()


def pytest_sessionfinish(session: pytest.Session, exitstatus: int):
  if session.sink_client:
    session.sink_client.close()


def _get_test_file(result: pytest.TestReport) -> str:
  (fspath, lineno, _) = result.location
  # File path uses the separator '/' regardless of the platform.
  fspath = fspath.replace(os.sep, '/')
  return f'//{fspath}#{lineno+1}'


def _extract_tags_from_properties(
    properties: List[Tuple[str, Tuple[str, str]]]) -> List[Tuple[str, str]]:
  """Extracts a list of (key, value) tuples used for test tags.

  Args:
    properties: The list of tuple in the format of (section_name, [key, value])

  Returns:
    A list of tuple (key, value) where the section name matches 'tag'
  """
  return [
    (key, value) for (sec, (key, value)) in properties
    if sec == _PROPERTY_SECTION_TAG
  ]


def _extract_artifacts_from_properties(
    properties: List[Tuple[str, Tuple[str, str]]]) -> Mapping[str, Any]:
  """Extracts a dict for artifacts attributes.

  Args:
    properties: The list of tuple in the format of
                ('artifact', [artifact_name, file_path])

  Returns:
    A dictionary of artifacts appropriate to passing to ResultSinkClient.Post
    and luci-go/resultdb/sink/proto/v1/test_result.proto.

    The dict is extracted from the property where the section name matches
    'artifact'.
  """
  return {
    artifact_name: {
      'filePath': file_path
    }
    for (sec, (artifact_name, file_path)) in properties
    if sec == _PROPERTY_SECTION_ARTIFACT
  }


@pytest.fixture
def add_artifact(request: pytest.FixtureRequest) -> AddArtifact:
  """The fixture that adds an artifact to the current test case."""
  def add(artifact_name: str, file_path: str) -> None:
    assert os.path.exists(file_path)
    request.node.user_properties.append(
      (_PROPERTY_SECTION_ARTIFACT, (artifact_name, file_path)))
  return add


@pytest.fixture
def add_tag(request: pytest.FixtureRequest) -> AddTag:
  """Fixture for adding a user defined tag to the current test case."""
  def add(tag_key: str, tag_value: str) -> None:
    request.node.user_properties.append(
      (_PROPERTY_SECTION_TAG, (tag_key, tag_value))
    )
  return add


def _report_test_result(result: pytest.TestReport,
                        item: pytest.Item,
                        call: pytest.CallInfo):
  test_file = _get_test_file(result)
  logging.info(f'posting result: {result.nodeid}, {result.duration}, '
                f'{_RESULT_TYPES[result.outcome]}, {test_file}, '
                f'{item.user_properties}')
  if failure_full := result.longreprtext:
    failure_reason = str(call.excinfo.getrepr(style='short'))
    b64_failure = base64.b64encode(failure_full.encode()).decode()
    artifacts = {'Failure Log': {'contents': b64_failure}}
  else:
    failure_reason = None
    artifacts = {}
  artifacts.update(_extract_artifacts_from_properties(item.user_properties))
  if sink_client := item.session.sink_client:
    tags = _extract_tags_from_properties(item.user_properties)
    sink_client.Post(result.nodeid, _RESULT_TYPES[result.outcome],
                     result.duration, result.caplog, test_file, tags=tags,
                     failure_reason=failure_reason, artifacts=artifacts)


@pytest.hookimpl(tryfirst=True, hookwrapper=True)
def pytest_runtest_makereport(item: pytest.Item, call: pytest.CallInfo):
  outcome = yield
  result: pytest.TestReport = outcome.get_result()

  # Some tags and artifacts will be added after 'call' and before 'teardown'
  if result.when == 'teardown':
    _report_test_result(result, item, call)