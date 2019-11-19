#!/usr/bin/env python
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Simple script to automatically download all current golden images for Android
# render tests or upload any newly generated ones.

import argparse
import hashlib
import multiprocessing
import os
import subprocess


STORAGE_BUCKET = 'chromium-android-render-test-goldens'
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..', '..'))
GOLDEN_DIRECTORIES = [
  os.path.join(THIS_DIR, 'render_tests'),
  os.path.join(
      CHROMIUM_SRC, 'components', 'test', 'data', 'js_dialogs', 'render_tests'),
  os.path.join(
      CHROMIUM_SRC, 'components', 'test', 'data', 'payments', 'render_tests'),
  os.path.join(
      CHROMIUM_SRC, 'components', 'test', 'data', 'permission_dialogs',
      'render_tests'),
  os.path.join(
      CHROMIUM_SRC, 'components', 'test', 'data', 'vr_browser_ui',
      'render_tests'),
  os.path.join(
      CHROMIUM_SRC, 'components', 'test', 'data', 'vr_browser_video',
      'render_tests'),
]

# This is to prevent accidentally uploading random, non-golden images that
# might be in the directory.
ALLOWED_DEVICE_SDK_COMBINATIONS = [
  # From RenderTestRule.java
  'Nexus_5-19',
  'Nexus_5X-23',
  # For VR tests.
  'Pixel_XL-25',
  'Pixel_XL-26',
]


# Assume a quad core if we can't get the actual core count.
try:
  THREAD_COUNT = multiprocessing.cpu_count()
  # cpu_count only gets the physical core count. There doesn't appear to be a
  # simple way of determining whether a CPU supports simultaneous multithreading
  # in Python, so assume that anything with 6 or more cores supports it.
  if THREAD_COUNT >= 6:
    THREAD_COUNT *= 2
except NotImplementedError:
  THREAD_COUNT = 4


def download(directory):
  # Downloading the files can be very spammy, so only show the output if
  # something actually goes wrong.
  try:
    subprocess.check_output([
        'download_from_google_storage',
        '--bucket', STORAGE_BUCKET,
        '-d', directory,
        '-t', str(THREAD_COUNT),
    ], stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    print ('Downloading RenderTest goldens in directory %s failed with error '
           '%d: %s') % (directory, e.returncode, e.output)


def upload(directory):
  def is_file_of_interest(f):
    if not f.endswith('.png'):
      return False
    for combo in ALLOWED_DEVICE_SDK_COMBINATIONS:
      if combo in f:
        return True
    return False

  files_to_upload = []
  for f in os.listdir(directory):
    # Skip any files that we don't care about.
    if not is_file_of_interest(f):
      continue

    png_path = os.path.join(directory, f)
    # upload_to_google_storage will upload a file even if it already exists
    # in the bucket. As an optimization, hash locally and only pass files to
    # the upload script if they don't have a matching .sha1 file already.
    sha_path = png_path + '.sha1'
    if os.path.isfile(sha_path):
      with open(sha_path) as sha_file:
        with open(png_path, 'rb') as png_file:
          h = hashlib.sha1()
          h.update(png_file.read())
          if sha_file.read() == h.hexdigest():
            continue
    files_to_upload.append(png_path)

  if len(files_to_upload):
    subprocess.check_call([
        'upload_to_google_storage.py',
        '--bucket', STORAGE_BUCKET,
        '-t', str(THREAD_COUNT),
    ] + files_to_upload)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('action', choices=['download', 'upload'],
                      help='Which action to perform')
  args = parser.parse_args()

  if args.action == 'download':
    for d in GOLDEN_DIRECTORIES:
      download(d)
  else:
    for d in GOLDEN_DIRECTORIES:
      upload(d)


if __name__ == '__main__':
  main()
