#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Find and fix files with inconsistent line endings.

This script requires 'dos2unix.exe' and 'unix2dos.exe' from Cygwin; they
must be in the user's PATH.

Arg: Either one or more files to examine, or (with --file-list) one or more
    files that themselves contain lists of files. The argument(s) passed to
    this script, as well as the paths in the file if any, may be relative or
    absolute Windows-style paths (with either type of slash). The list might
    be generated with 'find -type f' or extracted from a gcl change listing,
    for example.
"""

import errno
import logging
import optparse
import subprocess
import sys


# Whether to produce excessive debugging output for each file in the list.
DEBUGGING = False


class Error(Exception):
  """Local exception class."""
  pass


def CountChars(text, str):
  """Count the number of instances of the given string in the text."""
  split = text.split(str)
  logging.debug(len(split) - 1)
  return len(split) - 1


def PrevailingEOLName(crlf, cr, lf):
  """Describe the most common line ending.

  Args:
    crlf: How many CRLF (\r\n) sequences are in the file.
    cr: How many CR (\r) characters are in the file, excluding CRLF sequences.
    lf: How many LF (\n) characters are in the file, excluding CRLF sequences.

  Returns:
    A string describing the most common of the three line endings.
  """
  most = max(crlf, cr, lf)
  if most == cr:
    return 'cr'
  if most == crlf:
    return 'crlf'
  return 'lf'


def FixEndings(file, crlf, cr, lf):
  """Change the file's line endings to CRLF or LF, whichever is more common."""
  most = max(crlf, cr, lf)
  if most == crlf:
    result = subprocess.call('unix2dos.exe %s' % file, shell=True)
    if result:
      raise Error('Error running unix2dos.exe %s' % file)
  else:
    result = subprocess.call('dos2unix.exe %s' % file, shell=True)
    if result:
      raise Error('Error running dos2unix.exe %s' % file)


def ProcessFiles(filelist):
  """Fix line endings in each file in the filelist list."""
  for filename in filelist:
    filename = filename.strip()
    logging.debug(filename)
    try:
      # Open in binary mode to preserve existing line endings.
      text = open(filename, 'rb').read()
    except IOError, e:
      if e.errno != errno.ENOENT:
        raise
      logging.warning('File %s not found.' % filename)
      continue

    crlf = CountChars(text, '\r\n')
    cr = CountChars(text, '\r') - crlf
    lf = CountChars(text, '\n') - crlf

    if options.force_lf:
      if crlf > 0 or cr > 0:
        print '%s: forcing to LF' % filename
        # Fudge the counts to force switching to LF.
        FixEndings(filename, 0, 0, 1)
    else:
      if ((crlf > 0 and cr > 0) or
          (crlf > 0 and lf > 0) or
          (  lf > 0 and cr > 0)):
        print '%s: mostly %s' % (filename, PrevailingEOLName(crlf, cr, lf))
        FixEndings(filename, crlf, cr, lf)


def process(options, args):
  """Process the files."""
  if not args or len(args) < 1:
    raise Error('No files given.')

  if options.file_list:
    for arg in args:
      filelist = open(arg, 'r').readlines()
      ProcessFiles(filelist)
  else:
    filelist = args
    ProcessFiles(filelist)
  return 0


def main():
  if DEBUGGING:
    debug_level = logging.DEBUG
  else:
    debug_level = logging.INFO
  logging.basicConfig(level=debug_level,
                      format='%(asctime)s %(levelname)-7s: %(message)s',
                      datefmt='%H:%M:%S')

  option_parser = optparse.OptionParser()
  option_parser.add_option("", "--file-list", action="store_true",
                           default=False,
                           help="Treat the arguments as files containing "
                                "lists of files to examine, rather than as "
                                "the files to be checked.")
  option_parser.add_option("", "--force-lf", action="store_true",
                           default=False,
                           help="Force any files with CRLF to LF instead.")
  options, args = option_parser.parse_args()
  return process(options, args)


if '__main__' == __name__:
  sys.exit(main())
