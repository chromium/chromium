# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logger utility functions."""

import logging
import os

from test.integration_tests.common import path_finder

_DEFAULT_LOG_FORMAT = ('[%(levelname)-5s %(asctime)s.%(msecs)d '
                       '%(process)d:%(thread)d '
                       '%(filename)s:%(lineno)d]%(message)s')
_DATE_FORMAT = '%Y-%m-%d %H:%M:%S'


def GetLogLocation():
  log_dir = os.getenv('${ISOLATED_OUTDIR}')
  if log_dir is None:
    log_dir = os.path.join(path_finder.get_integration_tests_dir(),
                           'test_output')
    if not os.path.exists(log_dir):
      os.makedirs(log_dir)
  return log_dir

def LoggingToFile(log_file, log_level=logging.NOTSET):
  """Add a file log handler to the root log.

  Args:
    log_file: The log file path.
    log_level: Log level of the file handler.
  """
  # Always create a log file first for permission settings.
  open(log_file, 'a').close()

  logger = logging.getLogger()
  # Don't filter logs out at the logger level, let handler do the filter.
  logger.setLevel(logging.NOTSET)

  handler = logging.FileHandler(log_file)
  formatter = logging.Formatter(_DEFAULT_LOG_FORMAT, _DATE_FORMAT)
  handler.setFormatter(formatter)
  handler.setLevel(log_level)
  logger.addHandler(handler)
