# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contextlib import contextmanager
from typing import Optional

class DriverFactory:
  """The factory to create webdriver for the pre-defined environment"""

  @contextmanager
  def create_driver(self,
                    seed_file: Optional[str] = None,
                    options: Optional['webdriver.ChromeOptions'] = None
    ) -> 'webdriver.Remote':
    """Creates a webdriver."""
    raise NotImplemented

  def close(self):
    """Cleans up anything that is created during the session."""
    pass
