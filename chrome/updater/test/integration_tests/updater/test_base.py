# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base test class for updater testing.

This is the base class that all tests derive from. It extends unittest.TestCase
class to create the unittest-driven framework. It provides test fixtures through
setUp() and teardown() which can be overridden by test cases to perform their
intended action before test run and after test run respectively.
"""

import logging
import os
import typ

from test.integration_tests.common import logger_util
from test.integration_tests.updater.lib import updater_util


class UpdaterTestBase(typ.TestCase):
  """Base test class for updater testing on new infra."""

  @classmethod
  def setUpClass(cls):
    super(UpdaterTestBase, cls).setUpClass()

    # Sets up logging with the given level.
    log_file_path = os.path.join(logger_util.GetLogLocation(),
                                 'TestCase.log')
    log_level = logging.NOTSET
    logger_util.LoggingToFile(log_file_path, log_level)

  @classmethod
  def tearDownClass(cls):
    super(UpdaterTestBase, cls).tearDownClass()

  def setUp(self):
    super(UpdaterTestBase, self).setUp()
    logging.info('Running test: %s', self.id())
    logging.info('In test case setUp.')

    # cleanup the system before running test
    self.Clean()

    # assert for cleanliness
    self.ExpectClean()

    # set up mock server
    self.SetUpMockServer()
    # start mock server
    self.StartMockServer()

  def tearDown(self):
    logging.info('In test case tearDown.')
    # cleanup after test finishes.
    self.Clean()
    self.ExpectClean()

    super(UpdaterTestBase, self).tearDown()

  def Clean(self):
    """Removes Chrome and Updater from system."""
    updater_util.Clean()

  def ExpectClean(self):
    """Checks if the requested apps are uninstalled from system."""
    updater_util.ExpectClean()

  def SetUpMockServer(self):
    """Prepares mock server for the test run."""
    raise NotImplementedError('Test Case must set up mock server for its use.')

  def StartMockServer(self):
    """Starts mock server on the local system."""
    pass

  def GetExecutablePath(self):
    updater_util.GetExecutablePath()

  def GetInstallerPath(self):
    updater_util.GetInstallerPath()
