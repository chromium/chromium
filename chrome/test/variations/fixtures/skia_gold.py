# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import base64
import functools
import os
import shutil
import subprocess
import sys

import attr
import pytest
from selenium.webdriver.remote.webelement import WebElement
from typing import Tuple

# The module skia_gold_common is relative to its own path, add "build" dir
# to the search path.
from chrome.test.variations.test_utils import SRC_DIR
sys.path.append(os.path.join(SRC_DIR, 'build'))
from skia_gold_common import skia_gold_properties as sgp
from skia_gold_common import skia_gold_session_manager as sgsm
from skia_gold_common.skia_gold_session import SkiaGoldSession

_CORPUS ='finch-smoke-tests'

@functools.lru_cache
def _get_skia_gold_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser()
  sgp.SkiaGoldProperties.AddCommandLineArguments(parser)
  skia_options, _ = parser.parse_known_args()
  return skia_options


@attr.attrs()
class VariationsSkiaGoldUtil:
  """Wrapper test util class for skia gold API."""
  img_dir: str = attr.attrib()
  skia_gold_session: SkiaGoldSession = attr.attrib()
  test_name: str = attr.attrib()
  use_luci: bool = attr.attrib()

  def _png_file_for_name(self, name: str) -> str:
    """Returns a file name that should be used for diff comparison."""
    return os.path.join(self.img_dir, f'{name}.png')

  @staticmethod
  def screenshot_from_element(ele: WebElement) -> bytes:
    """Convenient method to screenshot an selement."""
    return base64.b64decode(ele.screenshot_as_base64)

  def compare(self, name: str, png_data: bytes) -> Tuple[int, str]:
    """Compares image using skia gold API.

    It saves png data into a file first and compares using `goldctl`. The image
    can be inspected after test runs. It runs locally or on a bot, and only
    upload gold images for triaging on a bot.
    """
    png_file = self._png_file_for_name(name)
    with open(png_file, "wb") as f:
      f.write(png_data)

    image_name = f'{self.test_name}:{name}'
    status, error_msg = self.skia_gold_session.RunComparison(
      name=image_name, png_file=png_file, use_luci=self.use_luci)
    return status, (
      f'{error_msg}'
      f'{self.skia_gold_session.GetTriageLinks(image_name)}'
    )

@pytest.fixture
def skia_gold_util(
  request: pytest.FixtureRequest,
  tmp_path_factory: pytest.TempPathFactory
  ) -> VariationsSkiaGoldUtil:
  """Returns VariationsSkiaGoldUtil to help compare gold images."""

  # Doesn't generate skia name for parameterization yet
  assert not hasattr(request, 'param')

  skia_tmp_dir = tmp_path_factory.mktemp('skia_gold', True)
  skia_img_dir = tmp_path_factory.mktemp('skia_img', True)

  skia_gold_properties = sgp.SkiaGoldProperties(
    args=_get_skia_gold_args())
  skia_gold_session_manager = sgsm.SkiaGoldSessionManager(
    skia_tmp_dir,
    skia_gold_properties
  )

  config = request.config
  session = skia_gold_session_manager.GetSkiaGoldSession({
    'platform': config.getoption('target_platform'),
    'channel': config.getoption('channel'),
  }, _CORPUS)

  test_file = os.path.relpath(request.module.__file__, SRC_DIR)

  try:
    util = VariationsSkiaGoldUtil(
      img_dir=skia_img_dir,
      skia_gold_session=session,
      test_name=f'{test_file}:{request.node.name}',
      use_luci=(not skia_gold_properties.local_pixel_tests))
    yield util
  finally:
    # TODO(b/280321923):
    # Keep the img dir in case of test failures when running locally.
    shutil.rmtree(skia_tmp_dir, ignore_errors=True)
    shutil.rmtree(skia_img_dir, ignore_errors=True)
