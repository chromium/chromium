# Copyright (C) 2024 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
    str = self.field_name + " {\n"
    for (key, value) in dict_items:
      if not isinstance(value, MetadataDictionary):
        str += (" " * width * depth) + f"{key}: {value}\n"
      else:
        str += (" " * width * depth) + value._as_string(value.items(), width,
                                                        depth + 1)
    str += (" " * width * (depth - 1)) + "}\n"
    return str

  def __repr__(self):
    return self._as_string(self.items())
