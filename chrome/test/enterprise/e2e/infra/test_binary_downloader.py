# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Download the required test binaries such as chrome installer and
   chromedriver for test args

    Prerequisite:
      gcloud auth login

    Usage:
      vpython3 test_binary_downloader.py --download_path '<absolute path>' \
      --channel 'CANARY'
"""

import logging
import os
import requests
import subprocess
import zipfile
from absl import app
from absl import flags

FLAGS = flags.FLAGS

flags.DEFINE_string('download_path', None,
                    'Absolute path to the download folder.')
flags.mark_flag_as_required('download_path')

flags.DEFINE_string('channel', 'CANARY', 'Chrome Channel to download.')


def get_latest_chrome_version(channel):
  """Get the latest Chrome version with specified channel"""
  if channel not in ["STABLE", "BETA", "DEV", "CANARY"]:
    raise ValueError("Invalid channel: {}".format(channel))

  url = 'https://googlechromelabs.github.io/chrome-for-testing/LATEST_RELEASE_' + channel
  response = requests.get(url)
  return response.text


def download_chromedriver(version, download_path):
  url = 'https://storage.googleapis.com/chrome-for-testing-public/{}/win64/chromedriver-win64.zip'.format(
      version)
  response = requests.get(url)
  if response.status_code == 200:
    # Save the zip file
    zip_file_path = os.path.join(download_path, 'chromedriver-win64.zip')
    with open(zip_file_path, 'wb') as f:
      f.write(response.content)

    # Extract the zip file
    with zipfile.ZipFile(zip_file_path, 'r') as zip_ref:
      zip_ref.extractall(download_path)
    logging.info("Chromedriver zip file downloaded and extract successfully")
  else:
    logging.info(
        f"Failed to download zip file from '{url}'. Status code: {response.status_code}"
    )


def download_chrome(version, download_path):
  gsutil_uri = 'gs://chrome-signed/desktop-5c0tCh/{}/win64-clang/mini_installer.exe.outputs/GoogleChromeStandaloneEnterprise.msi'.format(
      version)
  command = ['gsutil', 'cp', gsutil_uri, download_path]
  try:
    subprocess.run(command, check=True)
  except subprocess.CalledProcessError as e:
    logging.error(f"Error: {e}")


def main(argv):
  if not FLAGS.download_path:
    print("Error: Please specify the download path using --download_path")
    return
  download_path = FLAGS.download_path
  if not os.path.exists(download_path):
    os.makedirs(download_path)

  logging.info('Chrome channel: %s', FLAGS.channel)
  version = get_latest_chrome_version(FLAGS.channel)
  logging.info("Chrome version: {}".format(version))
  download_chromedriver(version, download_path)
  logging.info("Chromedriver downloaded")
  download_chrome(version, download_path)
  logging.info("Chrome downloaded")


if __name__ == '__main__':
  app.run(main)
