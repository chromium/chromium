# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class FrameReference:
  """Represents a frame."""
  def __init__(self, chromedriver, id_):
    self._chromedriver = chromedriver
    self._id = id_
