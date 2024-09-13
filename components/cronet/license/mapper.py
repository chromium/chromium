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

from typing import Dict, List, Union


class MapperException(Exception):
  # Raised when the Mapper expectation equality fails.
  pass


class Mapper:
  """
  This will overwrite the value of the metadata field whose key is |dictionary_key|
  with the value declared in |write_value|, before the overwrite happens, it
  will check if the expected_value matches the value in the metadata field.

  Passing None to the |expected_value| will expect that the key does not exist
  at all in the metadata.
  """

  def __init__(self, dictionary_key: str, expected_value, write_value):
    self._key = dictionary_key
    self._expected_value = expected_value
    self._write_value = write_value

  def write(self,
            metadata: Dict[str, Union[str, List[str]]]) -> None:
    """
    Writes the value |write_value| which is passed in the constructor of
    the Mapper to the |metadata| provided. Before the write operation happens,
    a check will occur to make sure that the value being overwritten matches
    the |expected_value| that was passed in the constructor.

    If |None| was passed as |expected_value| then the expectation is that
    the |key| should not exist in metadata.keys().

    :param metadata: A dictionary whose field with key |key| will be overwritten.
    :raises: MapperException if the expectation check has failed.
    """
    if self._expected_value is None and self._key in metadata:
      # We expected the key not to exist but it existed.
      raise MapperException(
          f"Expected absence of key `{self._key}` but "
          f"found {metadata[self._key]}")

    if self._key not in metadata.keys() and self._expected_value:
      # We expected a value but the key didn't exist, throw!
      raise MapperException(f"Expected presence of key {self._key} but was"
                            f"not found.")

    if self._key in metadata.keys() and metadata[
      self._key] != self._expected_value:
      # We expected the value to match but it didn't.
      raise MapperException(
          f"Expected \"{self._expected_value}\" but found"
          f" {metadata[self._key]} in the README.chromium")

    metadata[self._key] = self._write_value
