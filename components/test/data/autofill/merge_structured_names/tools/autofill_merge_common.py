# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class UnknownColumnNameException(Exception):
  """Exception type raised when encountering an unknown column name."""
  def __init__(self, column_name):
    self.column_name = column_name
  def __str__(self):
    return repr(self.column_name)


def SerializeProfiles(profiles):
  """Returns a serialized string for the given |profiles|.

  |profiles| should be a list of (field_type, value) string pairs.

  """

  lines = []
  for profile in profiles:
    # Include a fixed string to separate profiles.
    lines.append("---")
    for (field_type, value) in profile:
      if field_type == "ignored":
        continue;

      lines.append("%s:%s%s" % (field_type, (' ' if value else ''), value))

  return '\n'.join(lines)


def ColumnNameToFieldType(column_name):
  """Converts the given |column_name| to the corresponding AutofillField type.

  |column_name| should be a string drawn from the column names of the
  autofill_profiles table in the Chromium "Web Data" database.

  """

  column_name = column_name.lower()
  field_type = "unknown"
  if column_name in ["guid", "label", "country", "date_modified", "origin",
      "language_code", "use_count", "use_date", "sorting_code",
      "dependent_locality"]:
    field_type = "ignored"
  elif column_name == "first_name":
    field_type = "NAME_FIRST"
  elif column_name == "middle_name":
    field_type = "NAME_MIDDLE"
  elif column_name == "last_name":
    field_type = "NAME_LAST"
  elif column_name == "full_name":
    field_type = "NAME_FULL"
  elif column_name == "email":
    field_type = "EMAIL_ADDRESS"
  elif column_name == "company_name":
    field_type = "COMPANY_NAME"
  elif column_name == "address_line_1":
    field_type = "ADDRESS_HOME_LINE1"
  elif column_name == "address_line_2":
    field_type = "ADDRESS_HOME_LINE2"
  elif column_name == "street_address":
    field_type = "ADDRESS_HOME_STREET_ADDRESS"
  elif column_name == "city":
    field_type = "ADDRESS_HOME_CITY"
  elif column_name == "state":
    field_type = "ADDRESS_HOME_STATE"
  elif column_name == "zipcode":
    field_type = "ADDRESS_HOME_ZIP"
  elif column_name == "country_code":
    field_type = "ADDRESS_HOME_COUNTRY"
  elif column_name == "phone":
    field_type = "PHONE_HOME_WHOLE_NUMBER"
  else:
    raise UnknownColumnNameException(column_name)

  return field_type
