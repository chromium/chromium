#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sqlite3
import sys
import time
import uuid


# Database Default File Name
_WEB_DATA_DB = 'Web Data'

# Table Names
_AUTOFILL_PROFILE_TABLE = 'autofill_profiles'
_AUTOFILL_PROFILE_NAMES_TABLE = 'autofill_profile_names'
_AUTOFILL_PROFILE_EMAILS_TABLE = 'autofill_profile_emails'
_AUTOFILL_PROFILE_PHONES_TABLE = 'autofill_profile_phones'
_AUTOFILL_CREDIT_CARDS_TABLE = 'credit_cards'

# Other constants
_185_DAYS_IN_SECONDS = 185 * 24 * 60 * 60


def open_autofill_db(profile_dir):
  db = sqlite3.connect(os.path.join(profile_dir, _WEB_DATA_DB))
  db.isolation_level = None
  return db


def insert(cursor, table, data_dict):
  """Uses |cursor| to insert |data| into |table|."""
  statement = \
      'INSERT INTO \'{table}\' ({fields}) VALUES ({tokens})'.format(
          table=table,
          fields=','.join(k for k in data_dict.iterkeys()),
          tokens=','.join('?' for _ in data_dict.iterkeys()))
  cursor.execute(statement, data_dict.values())


def add_disused_address(cursor):
  """Add a disused addres profile to the database (via |cursor|)"""
  guid=str(uuid.uuid4())
  timestamp = int(time.time()) - _185_DAYS_IN_SECONDS
  profile_data = dict(guid=guid,
                      company_name='%s Test Inc.' % timestamp,
                      street_address='123 Invented Street\nSuite A',
                      dependent_locality='',
                      city='Mountain View',
                      state='California',
                      zipcode='94043',
                      sorting_code='',
                      country_code='US',
                      use_count=13,
                      use_date=timestamp,
                      date_modified=timestamp,
                      origin='https://test.chromium.org',
                      language_code='en')
  name_data = dict(guid=guid, first_name='John', middle_name='Quincy',
                   last_name='Disused', full_name='John Quincy Disused')
  email_data = dict(guid=guid, email='disused@fake.chromium.org')
  phone_data = dict(guid=guid, number='800-555-0173')
  insert(cursor, _AUTOFILL_PROFILE_TABLE, profile_data)
  insert(cursor, _AUTOFILL_PROFILE_NAMES_TABLE, name_data)
  insert(cursor, _AUTOFILL_PROFILE_EMAILS_TABLE, email_data)
  insert(cursor, _AUTOFILL_PROFILE_PHONES_TABLE, phone_data)
  return guid


def main():
  if len(sys.argv) != 2:
    print "Usage: python add_disused_address.py <profle_dir>"
    return 1

  database = open_autofill_db(sys.argv[1])
  cursor = database.cursor()
  cursor.execute('BEGIN')
  add_disused_address(cursor)
  cursor.execute('COMMIT')
  print "Added one disused address."


if __name__ == '__main__':
  sys.exit(main())
