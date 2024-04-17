# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script lib to download all current files from GCS or upload any newly
# generated ones.

import hashlib
import logging
import multiprocessing
import os
import subprocess


def _get_thread_count():
  """Gets a thread_count based on the multiprocessing.cpu_count()."""
  try:
    thread_count = multiprocessing.cpu_count()
    # cpu_count only gets the physical core count. There doesn't appear to be a
    # simple way of determining whether a CPU supports simultaneous
    # multithreading in Python, so assume that anything with 6 or more cores
    # supports it.
    if thread_count >= 6:
      thread_count *= 2
  except NotImplementedError:
    # Assume a quad core if we can't get the actual core count.
    thread_count = 4
  return thread_count


def download(directory, filter, scenario, gcs_bucket):
  """Downloads the files from cloud as according to the given directory.

  Args:
    directory: A local disk directory.
    filter: A method to filter target files inside this directory.
    scenario: A scenario names that utilize this download method.
    gcs_bucket: A GCS bucket name to be used in the download.
  """
  # If someone removes a SHA1 file, we want to remove the associated cloud file
  # the next time file are updated.
  files_to_delete = _get_files_to_delete(directory, filter)
  for file_path in files_to_delete:
    os.remove(file_path)

  # Downloading the files can be very spammy, so only show the output if
  # something actually goes wrong.
  try:
    subprocess.check_output([
        'download_from_google_storage',
        '--bucket', gcs_bucket,
        '-d', directory,
        '-t', str(_get_thread_count()),
    ], stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    logging.error('Downloading %s in directory %s failed with error '
                  '%d: %s', scenario, directory, e.returncode, e.output)


# TODO(crbug.com/40700852): Remove this after root cause is found.
def verify_file_exists(directory, filter):
  """Verifies that some file exists in the directory."""
  for _, _, file_list in os.walk(directory):
    for f in file_list:
      if filter(f):
        return True
  return False


def _get_files_to_delete(directory, filter):
  """Returns a list of local files to delete.

  Args:
    directory: A local disk directory.
    filter: A method to filter target files inside the directory.
  """
  files_to_delete = []
  for f in os.listdir(directory):
    if not filter(f):
      continue
    sha1_path = os.path.join(directory, f + '.sha1')
    if not os.path.exists(sha1_path):
      files_to_delete.append(os.path.join(directory, f))
  return files_to_delete


def upload(directory, filter, scenario, gcs_bucket, dry_run=False):
  """Uploads the files to cloud storage.

  Args:
    directory: A local disk directory.
    filter: A method to filter target files inside the directory.
    scenarios: A scenario that calls this upload.
    gcs_bucket: A GCS bucket name to be used in upload.
    dry_run: A flag to indicate whether it is just a dry_run or a real upload.
  """
  files_to_upload = _get_files_to_upload(directory, filter)

  if len(files_to_upload):
    if dry_run:
      logging.info('Will upload the following %s: %s',
                   scenario, ', '.join(files_to_upload))
      logging.info('Destination gcs bucket: %s', gcs_bucket)
      return
    subprocess.check_call([
        'upload_to_google_storage.py',
        '--bucket', gcs_bucket,
        '-t', str(_get_thread_count()),
    ] + files_to_upload)


def _get_files_to_upload(directory, filter):
  """Returns a list of files to upload to cloud storage.

  Args:
    directory: A local disk directory.
    filter: A method to filter target files inside the directory.
  """
  files_to_upload = []
  for f in os.listdir(directory):
    # Skip any files that we don't care about.
    if not filter(f):
      continue

    file_path = os.path.join(directory, f)
    # upload_to_google_storage will upload a file even if it already exists
    # in the bucket. As an optimization, hash locally and only pass files to
    # the upload script if they don't have a matching .sha1 file already.
    sha_path = file_path + '.sha1'
    if os.path.isfile(sha_path):
      with open(sha_path) as sha_file:
        with open(file_path, 'rb') as source_file:
          h = hashlib.sha1()
          h.update(source_file.read())
          if sha_file.read() == h.hexdigest():
            continue
    files_to_upload.append(file_path)
  return files_to_upload
