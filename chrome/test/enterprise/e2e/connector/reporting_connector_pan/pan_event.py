# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from typing import Any

import attrs


@attrs.frozen(kw_only=True)
class PanEvent(object):
  """Maps to a event reported to the pan console.

  This class stores a event that is to match a xdr queried event from pan.

  Attributes:
    type: The "event"/"event" field in the dataset table. Defaults to None.
    device_id: The "event"/"device_id" field in the dataset table. Defaults to
      None.
    reason: The "event"/"reason" field in the dataset table. Defaults to None.
    url: The "event"/"url" field in the dataset table. Defaults to None.
  """

  type: str = None
  device_id: str = None
  reason: str = None
  url: str = None

  def __eq__(self, other: Any) -> bool:
    if self is other:
      return True
    if not isinstance(other, self.__class__):
      return False
    return (other.type == self.type and other.device_id == self.device_id and
            other.reason == self.reason and other.url == self.url)
