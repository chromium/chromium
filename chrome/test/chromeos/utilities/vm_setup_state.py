#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import pyauto_functional # has to be imported before pyauto
import pyauto
import sys

VM_CHROMEDRIVER_PORT = 4444

if __name__ == '__main__':
  """Script to prepare machine state for use as a WebDriver-controlled VM.

  This script is intended to be run manually over ssh on a Chromium OS virtual
  machine qcow2 image. Manually create a snapshot of the VM when prompted. The
  resulting VM image will have ChromeDriver listening on port 4444.
  """
  pyauto_suite = pyauto.PyUITestSuite(sys.argv)
  pyuitest = pyauto.PyUITest()
  pyuitest.setUp()
  driver = pyuitest.NewWebDriver(port=VM_CHROMEDRIVER_PORT)
  logging.info('WebDriver is listening on port %d.'
               % VM_CHROMEDRIVER_PORT)
  logging.info('Machine prepared for VM snapshot.')
  raw_input('Please snapshot the VM and hit ENTER when done to '
            'terminate this script.')
  pyuitest.tearDown()
  del pyuitest
  del pyauto_suite
