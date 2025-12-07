# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class MetadataDictionary(dict):
    """
  This is a very simple class that prints out a textproto using a dictionary.
  Realistically, we should not be re-inventing the wheel as we are doing here
  and we should be using protobuf instead.

  TODO(b/360322121): Use protobuf generated classes instead of this.
  """

    def __init__(self, field_name):
        super().__init__()
        self.field_name = field_name

    def _as_string(self, dict_items, width=2, depth=1):
        string = self.field_name + " {\n"
        for (key, value) in dict_items:
            if not isinstance(value, MetadataDictionary):
                string += (" " * width * depth) + f"{key}: {value}\n"
            else:
                string += (" " * width * depth) + value._as_string(
                    value.items(), width, depth + 1)
        string += (" " * width * (depth - 1)) + "}\n"
        return string

    def __repr__(self):
        return self._as_string(self.items())
