# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from command_executor import Command

class WebShadowRoot(object):
  """Represents an HTML shadow root"""
  def __init__(self, chromedriver, id_):
    self._chromedriver = chromedriver
    self._id = id_

  def _Execute(self, command, params=None):
    if params is None:
        params = {}
    params['id'] = self._id
    return self._chromedriver.ExecuteCommand(command, params)

  def FindElement(self, strategy, target):
    return self._Execute(
      Command.FIND_ELEMENT_FROM_SHADOW_ROOT,
      {'using': strategy, 'value': target})

  def FindElements(self, strategy, target):
    return self._Execute(
      Command.FIND_ELEMENTS_FROM_SHADOW_ROOT,
      {'using': strategy, 'value': target})
