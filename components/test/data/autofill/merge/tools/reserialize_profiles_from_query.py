#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from autofill_merge_common import SerializeProfiles, ColumnNameToFieldType


def main():
  """Serializes the output of the query 'SELECT * from autofill_profiles;'.
  """

  COLUMNS = ['GUID', 'LABEL', 'FIRST_NAME', 'MIDDLE_NAME', 'LAST_NAME', 'EMAIL',
             'COMPANY_NAME', 'ADDRESS_LINE_1', 'ADDRESS_LINE_2', 'CITY',
             'STATE', 'ZIPCODE', 'COUNTRY', 'PHONE', 'DATE_MODIFIED']

  if len(sys.argv) != 2:
    print ("Usage: python reserialize_profiles_from_query.py "
           "<path/to/serialized_profiles>")
    return

  types = [ColumnNameToFieldType(column_name) for column_name in COLUMNS]
  profiles = []
  with open(sys.argv[1], 'r') as serialized_profiles:
    for line in serialized_profiles:
      # trim the newline if present
      if line[-1] == '\n':
        line = line[:-1]

      values = line.split("|")
      profiles.append(zip(types, values))

  print SerializeProfiles(profiles)
  return 0


if __name__ == '__main__':
  sys.exit(main())
