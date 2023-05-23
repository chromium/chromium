# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Downloader module to download various versions of Chrome from GCS bucket.

download_chrome_{platform}(version) returns a folder that contains Chrome
binary and chromedriver so it is ready to run a webdriver test.
"""

import functools
import json
import os
import shutil
import subprocess
import sys

from chrome.test.variations.test_utils import helper
from chrome.test.variations.test_utils import SRC_DIR
from pkg_resources import packaging
from typing import List
from urllib.request import urlopen

GSUTIL_PATH = os.path.join(
    SRC_DIR, 'third_party', 'catapult', 'third_party', 'gsutil', 'gsutil')

CHROME_DIR = os.path.join(SRC_DIR, "_chrome")


@functools.lru_cache
def _find_gsutil_cmd() -> str:
  if gsutil := (shutil.which('gsutil.py') or shutil.which('gsutil')):
    return gsutil
  if os.path.exists(GSUTIL_PATH):
    return GSUTIL_PATH
  raise RuntimeError("Please specify script path for gsutil or run "
                     "'sudo apt install google-cloud-sdk' and try again.")

def _download_chrome(version: str, files: List[str]) -> str:
  downloaded_dir = os.path.join(CHROME_DIR, version)
  os.makedirs(downloaded_dir, exist_ok=True)

  # TODO: we can compare the local md5 if existed to avoid repetitive downloads
  gs_cmd = [_find_gsutil_cmd(), "cp"]
  gs_cmd.extend([f'gs://chrome-unsigned/desktop-5c0tCh/{version}/{file}'
                 for file in files])
  gs_cmd.extend([downloaded_dir])

  if helper.get_hosted_platform() == "win":
    subprocess.run([sys.executable] + gs_cmd, check=False)
    for file in files:
      unzip_cmd = ["powershell", "-command",
                   "Expand-Archive -Force '%s'" %
                     os.path.join(downloaded_dir, os.path.basename(file)),
                   downloaded_dir]
      subprocess.run(unzip_cmd, check=False)

  else:
    subprocess.run(gs_cmd, check=False)

    for file in files:
      unzip_cmd = ["unzip",
                   "-o",
                   os.path.join(downloaded_dir, os.path.basename(file)),
                   "-d",
                   downloaded_dir]
      subprocess.run(unzip_cmd, capture_output=True, check=False)
  return downloaded_dir


def download_chromedriver_linux_host(channel: str, version: str) -> str:
  """Download the chromedriver that works with the given channel/version."""

  # Find the same or next version of the given channel and version whose
  # chromedriver is compatible with.
  # Linux doesn't distribute canary, use dev instead.
  if channel == 'canary':
    channel = 'dev'
  closest_version = find_closest_version(
    release_os='linux', channel=channel, version=version)
  downloaded_dir = _download_chrome(str(closest_version),
                                    ['linux64/chromedriver_linux64.zip'])

  return os.path.join(downloaded_dir, "chromedriver_linux64")


def download_chrome_mac(version: str) -> str:
  files = ["mac-universal/chrome-mac.zip", "mac-arm64/chromedriver_mac64.zip"]
  downloaded_dir = _download_chrome(version, files)
  shutil.move(
    os.path.join(downloaded_dir, "chromedriver_mac64", "chromedriver"),
    os.path.join(downloaded_dir, "chrome-mac", "chromedriver"))

  return os.path.join(downloaded_dir, "chrome-mac")

def download_chrome_win(version: str) -> str:
  files = ["win64-clang/chrome-win64-clang.zip",
           "win64-clang/chromedriver_win64.zip"]
  downloaded_dir = _download_chrome(version, files)
  shutil.move(
    os.path.join(downloaded_dir, "chromedriver_win64", "chromedriver.exe"),
    os.path.join(downloaded_dir, "chrome-win64-clang", "chromedriver.exe"))

  return os.path.join(downloaded_dir, "chrome-win64-clang")


def download_chrome_linux(version: str) -> str:
  files = ["linux64/chrome-linux64.zip", "linux64/chromedriver_linux64.zip"]
  downloaded_dir = _download_chrome(version, files)

  shutil.move(
    os.path.join(downloaded_dir, "chromedriver_linux64", "chromedriver"),
    os.path.join(downloaded_dir, "chrome-linux64", "chromedriver"))

  return os.path.join(downloaded_dir, "chrome-linux64")

def find_version(release_os: str, channel: str) -> packaging.version.Version:
  # See go/versionhistory-user-guide
  url = (
      f"https://versionhistory.googleapis.com/v1/chrome/"
      # Limit to just the platform and channel that we care about
      f"platforms/{release_os}/channels/{channel}/versions/all/releases"
      # There might be experiments where different versions are shipping to the
      # same channel, so order the results descending by the fraction so that
      # the first returned release is the standard release for the channel
      f"?order_by=fraction%20desc")

  try:
    response = json.loads(urlopen(url=url).read())
    return packaging.version.parse(response['releases'][0]['version'])
  except Exception as e:
    raise RuntimeError("Fail to retrieve version info.") from e

def find_closest_version(release_os: str,
                         channel: str,
                         version: str) -> packaging.version.Version:
  # See find_version
  # Search for the newest major version,
  # https://crsrc.org/c/chrome/test/chromedriver/chrome_launcher.cc;l=287;drc=ea1bdac
  major = int(version.split('.')[0])
  url = (
      f"https://versionhistory.googleapis.com/v1/chrome/"
      f"platforms/{release_os}/channels/{channel}/versions/all/releases"
      # Find the newest version that's earlier than the next major,
      # so effectively, it finds the latest of the current major.
      f"?filter=version<{major+1}"
      # Order by version descending to find the earliest/closest version.
      f"&order_by=version%20desc")

  try:
    response = json.loads(urlopen(url=url).read())
    return packaging.version.parse(response['releases'][0]['version'])
  except Exception as e:
    raise RuntimeError("Fail to retrieve version info.") from e
