# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dumps a Windows shortcut's property bag to stdout.

This is required to confirm correctness of properties that aren't readily
available in Windows UI.
"""

import optparse
from pywintypes import IID
import sys
from win32com.propsys import propsys
from win32com.propsys import pscon


def BuildPKeyToNameMapping():
  """Returns a dict mapping PKey values (a tuple of an IID and an int) to their
  names."""
  # The pscon module contains a number of well-known PKey values. Scan through
  # the module picking out anything that looks plausibly like a PROPERTYKEY (a
  # tuple of a PyIID and an int), and map it to its name in the module.
  return {item: name for (name, item) in pscon.__dict__.iteritems() if (
             isinstance(item, tuple) and
             len(item) == 2 and
             isinstance(item[1], int))}


def PrintShortcutProperties(shortcut_path, dump_all):
  properties = propsys.SHGetPropertyStoreFromParsingName(shortcut_path)

  print 'Known properties (--dump-all for more):'

  app_id = properties.GetValue(pscon.PKEY_AppUserModel_ID).GetValue()
  print '\tAppUserModelId => "%s"' % app_id

  # Hard code PKEY_AppUserModel_IsDualMode as pscon doesn't support it.
  PKEY_AppUserModel_IsDualMode = (IID('{9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3}'),
                                  11)
  dual_mode = properties.GetValue(PKEY_AppUserModel_IsDualMode).GetValue()
  print '\tDual Mode => "%s"' % dual_mode

  # Dump all other properties with their raw ID if requested, add them above
  # over time as we explicitly care about more properties, see propkey.h or
  # pscon.py for a reference of existing PKEYs' meaning.
  if dump_all:
    key_to_name = BuildPKeyToNameMapping()
    print '\nOther properties:'
    for i in range(0, properties.GetCount()):
      property_key = properties.GetAt(i)
      # |property_key| is a tuple of an IID identifying the format and an int
      # (a.k.a. a PROPERTYKEY struct). If this key is one of the predefined ones
      # in the pscon module, display the name given to it by the module. If not,
      # show the key's IID and int -- the viewer may be able to find it in
      # propkey.h.
      key_name = key_to_name.get(property_key, str(property_key))
      property_value = properties.GetValue(property_key).GetValue()
      print '\t%s => "%s"' % (key_name, property_value)


def main():
  usage = 'usage: %prog [options] "C:\\Path\\To\\My Shortcut.lnk"'
  parser = optparse.OptionParser(usage,
                                 description="Dumps a shortcut's  properties.")
  parser.add_option('-a', '--dump-all', action='store_true', dest='dump_all',
                    default=False)
  options, args = parser.parse_args()

  if len(args) != 1:
    parser.error('incorrect number of arguments')

  PrintShortcutProperties(args[0], options.dump_all)


if __name__ == '__main__':
  sys.exit(main())
