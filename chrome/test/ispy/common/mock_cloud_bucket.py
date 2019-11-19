# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Subclass of CloudBucket used for testing."""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
import cloud_bucket


class MockCloudBucket(cloud_bucket.BaseCloudBucket):
  """Subclass of CloudBucket used for testing."""

  def __init__(self):
    """Initializes the MockCloudBucket with its datastore.

    Returns:
      An instance of MockCloudBucket.
    """
    self.datastore = {}

  def Reset(self):
    """Clears the MockCloudBucket's datastore."""
    self.datastore = {}

  # override
  def UploadFile(self, path, contents, content_type):
    self.datastore[path] = contents

  # override
  def DownloadFile(self, path):
    if path in self.datastore:
      return self.datastore[path]
    else:
      raise cloud_bucket.FileNotFoundError

  # override
  def UpdateFile(self, path, contents):
    if not self.FileExists(path):
      raise cloud_bucket.FileNotFoundError
    self.UploadFile(path, contents, '')

  # override
  def RemoveFile(self, path):
    if path in self.datastore:
      self.datastore.pop(path)

  # override
  def FileExists(self, path):
    return path in self.datastore

  # override
  def GetImageURL(self, path):
    if path in self.datastore:
      return path
    else:
      raise cloud_bucket.FileNotFoundError

  # override
  def GetAllPaths(self, prefix):
    return (item[0] for item in self.datastore.items()
            if item[0].startswith(prefix))
