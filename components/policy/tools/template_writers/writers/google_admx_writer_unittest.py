#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for writers.google_admx_writer."""

import unittest
from writers import google_admx_writer


class GoogleAdmxWriterUnittest(unittest.TestCase):

  def setUp(self):
    self.writer = google_admx_writer.GetWriter(None)  # Config unused

  def testGoogleAdmx(self):
    output = self.writer.WriteTemplate(None)  # Template unused

    # No point to duplicate the full XML.
    self.assertTrue('namespace="Google.Policies"' in output)


if __name__ == '__main__':
  unittest.main()
