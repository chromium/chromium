# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import base64
import functools
import os
import shutil
import struct
import sys

import attr
import pytest
from selenium.webdriver.remote.webelement import WebElement
from typing import List, Tuple

# The module skia_gold_common is relative to its own path, add "build" dir
# to the search path.
from chrome.test.variations.test_utils import SRC_DIR
sys.path.append(os.path.join(SRC_DIR, 'build'))
from skia_gold_common import skia_gold_properties as sgp
from skia_gold_common import skia_gold_session_manager as sgsm
from skia_gold_common.skia_gold_session import SkiaGoldSession

from chrome.test.variations.fixtures.result_sink import AddTag
from chrome.test.variations.fixtures.test_options import TestOptions

_CORPUS ='finch-smoke-tests'

@functools.lru_cache
def _get_skia_gold_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser()
  sgp.SkiaGoldProperties.AddCommandLineArguments(parser)
  skia_options, _ = parser.parse_known_args()
  return skia_options


def _get_png_image_info(data: bytes) -> Tuple[int, int]:
  if _is_png(data):
    weight, height = struct.unpack('>LL', data[16:24])
    return int(weight), int(height)
  else:
    raise RuntimeError('not a png image')


def _is_png(data: bytes) -> bool:
  return data[:6] == b'\x89PNG\r\n' and data[12:16] == b'IHDR'


@attr.attrs()
class VariationsSkiaGoldUtil:
  """Wrapper test util class for skia gold API."""
  img_dir: str = attr.attrib()
  skia_gold_session: SkiaGoldSession = attr.attrib()
  test_name: str = attr.attrib()
  use_luci: bool = attr.attrib()
  request: pytest.FixtureRequest = attr.attrib()
  add_tag: AddTag = attr.attrib()

  def _png_file_for_name(self, name: str) -> str:
    """Returns a file name that should be used for diff comparison."""
    return os.path.join(self.img_dir, f'{name}.png')

  def _inexact_matching_args(self, max_diff: int) -> List[str]:
    # Fuzzy matching algorithms allow to diff in a certain number of pixels.
    # https://skia.googlesource.com/buildbot/+/main/gold-client/go/imgmatching/fuzzy/fuzzy.go
    return[
      '--add-test-optional-key',
      'image_matching_algorithm:fuzzy',
      # Some hardware may cause rasterization inconsistency, resulting two
      # completely different pixels. Here we tolerate some level of hardware
      # rendering on each pixel even those two pixels are completely different,
      # without compromising significant visual differences.
      '--add-test-optional-key',
      'fuzzy_pixel_per_channel_delta_threshold:255',
      '--add-test-optional-key',
      f'fuzzy_max_different_pixels:{max_diff}',
    ]

  @staticmethod
  def screenshot_from_element(ele: WebElement) -> bytes:
    """Convenient method to screenshot an selement."""
    return base64.b64decode(ele.screenshot_as_base64)

  def compare(self, name: str,
              png_data: bytes,
              threshold: float = 0.001) -> Tuple[int, str]:
    """Compares image using skia gold API.

    It saves png data into a file first and compares using `goldctl`. The image
    can be inspected after test runs. It runs locally or on a bot, and only
    upload gold images for triaging on a bot.

    Args:
      name: the image name used to identify the comparison result.
      png_data: the raw data saving png contents.
      threshold: the percentage to allow pixel differences.

    Returns:
      The tuple of status code and optional error message.

    Raises:
      RuntimeError if the png_data is not a PNG content.
    """
    weight, height = _get_png_image_info(png_data)
    max_pixel = int(weight * height * threshold)
    png_file = self._png_file_for_name(name)
    with open(png_file, "wb") as f:
      f.write(png_data)

    image_name = f'{self.test_name}:{name}'
    status, error_msg = self.skia_gold_session.RunComparison(
      name=image_name, png_file=png_file,
      inexact_matching_args=self._inexact_matching_args(max_pixel),
      use_luci=self.use_luci)

    # Screenshots for variations are in chrome-gold.skia.org
    triage_link = self.skia_gold_session.GetTriageLinks(image_name)[1]
    if triage_link:
      self.add_tag(f'skiagold_link/{name}', triage_link)

    return status, f'{error_msg} \n{triage_link}'

@pytest.fixture
def skia_gold_util(
  request: pytest.FixtureRequest,
  tmp_path_factory: pytest.TempPathFactory,
  test_options: TestOptions,
  add_tag: AddTag
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
    'platform': test_options.platform,
    'channel': test_options.channel,
  }, _CORPUS)

  test_file = os.path.relpath(request.module.__file__, SRC_DIR)

  try:
    util = VariationsSkiaGoldUtil(
      request=request,
      img_dir=skia_img_dir,
      skia_gold_session=session,
      add_tag=add_tag,
      test_name=f'{test_file}:{request.node.name}',
      use_luci=(not skia_gold_properties.local_pixel_tests))
    yield util
  finally:
    # TODO(b/280321923):
    # Keep the img dir in case of test failures when running locally.
    shutil.rmtree(skia_tmp_dir, ignore_errors=True)
    shutil.rmtree(skia_img_dir, ignore_errors=True)