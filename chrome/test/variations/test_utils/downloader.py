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
import packaging
from typing import List, Optional
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

def _download_files_from_gcs(version: str, files: List[str]) -> str:
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


def download_chromedriver(platform: str, version: Optional[str]) -> str:
  """Download the latest available chromedriver for the platform.

  Args:
    platform: The platform that chromedriver is running on
    version: The version of chromedriver. It will find the latest if None.

  Returns:
    the path to the chromedriver executable
  """

  # Linux doesn't have canary
  if platform == 'linux':
    channel = 'dev'
    release_os = 'linux'
    driver_pathname = 'linux64/chromedriver_linux64.zip'
  elif platform == 'win':
    channel = 'canary'
    release_os = 'win64'
    driver_pathname = 'win64-clang/chromedriver_win64.zip'
  elif platform == 'mac':
    channel = 'canary'
    release_os = 'mac_arm64'
    driver_pathname = 'mac-arm64/chromedriver_mac64.zip'
  else:
    assert False, f'Not supported platform {platform}'
  driver_zip_path = os.path.basename(driver_pathname)[:-4]

  if not version:
    version = find_version(release_os, channel)
  downloaded_dir = _download_files_from_gcs(str(version), [driver_pathname])

  hosted_platform = helper.get_hosted_platform()
  if hosted_platform == 'win':
    chromedriver_bin = 'chromedriver.exe'
  else:
    chromedriver_bin = 'chromedriver'
  chromedriver_path = os.path.join(downloaded_dir, chromedriver_bin)
  shutil.move(
    os.path.join(downloaded_dir, driver_zip_path, chromedriver_bin),
    chromedriver_path)
  return chromedriver_path


def download_chrome_mac(version: str) -> str:
  files = ['mac-universal/chrome-mac.zip']
  downloaded_dir = _download_files_from_gcs(version, files)
  return os.path.join(downloaded_dir, "chrome-mac")

def download_chrome_win(version: str) -> str:
  files = ['win64-clang/chrome-win64-clang.zip']
  downloaded_dir = _download_files_from_gcs(version, files)
  return os.path.join(downloaded_dir, "chrome-win64-clang")


def download_chrome_linux(version: str) -> str:
  files = ["linux64/chrome-linux64.zip"]
  downloaded_dir = _download_files_from_gcs(version, files)
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

def parse_version(version: str) -> packaging.version.Version:
  try:
    return packaging.version.parse(version)
  except packaging.version.InvalidVersion:
    raise RuntimeError(f"Invalid version: {version}")

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
