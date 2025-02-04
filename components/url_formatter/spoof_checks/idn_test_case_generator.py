#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for generating IDN test cases.

Either use the command-line interface (see --help) or directly call make_case
from Python shell (see make_case documentation).
"""


import argparse
import codecs
import doctest
import sys


def str_to_c_string(string):
  """Converts a Python bytes to a C++ string literal.

    >>> str_to_c_string(b'abc\x8c')
    '"abc\\\\x8c"'
    """
  return repr(string).replace("'", '"').removeprefix('b')


def unicode_to_c_ustring(string):
  """Converts a Python unicode string to a C++ u16-string literal.

    >>> unicode_to_c_ustring(u'b\u00fccher.de')
    'u"b\\\\u00fccher.de"'
    """
  result = ['u"']
  for c in string:
    if (ord(c) > 0xffff):
      escaped = '\\U%08x' % ord(c)
    elif (ord(c) > 0x7f):
      escaped = '\\u%04x' % ord(c)
    else:
      escaped = c
    result.append(escaped)
  result.append('"')
  return ''.join(result)


def make_case(unicode_domain, unicode_allowed=True, case_name=None):
  """Generates a C++ test case for an IDN domain test.

    This is designed specifically for the IDNTestCase struct in the file
    components/url_formatter/url_formatter_unittest.cc. It generates a row of
    the idn_cases array, specifying a test for a particular domain.

    |unicode_domain| is a Unicode string of the domain (NOT IDNA-encoded).
    |unicode_allowed| specifies whether the test case should expect the domain
    to be displayed in Unicode form (kSafe) or in IDNA/Punycode ASCII encoding
    (kUnsafe). |case_name| is just for the comment.

    This function will automatically convert the domain to its IDNA format, and
    prepare the test case in C++ syntax.

    >>> make_case(u'\u5317\u4eac\u5927\u5b78.cn', True, 'Hanzi (Chinese)')
        // Hanzi (Chinese)
        {"xn--1lq90ic7f1rc.cn", u"\\u5317\\u4eac\\u5927\\u5b78.cn", kSafe},
    >>> make_case(u'b\u00fccher.de', True)
        {"xn--bcher-kva.de", u"b\\u00fccher.de", kSafe},

    This will also apply normalization to the Unicode domain, as required by the
    IDNA algorithm. This example shows U+210F normalized to U+0127 (this
    generates the exact same test case as u'\u0127ello'):

    >>> make_case(u'\u210fello', True)
        {"xn--ello-4xa", u"\\u0127ello", kSafe},
    """
  idna_input = codecs.encode(unicode_domain, 'idna')
  # Round-trip to ensure normalization.
  unicode_output = codecs.decode(idna_input, 'idna')
  if case_name:
    print('    // %s' % case_name)
  print('    {%s, %s, %s},' %
        (str_to_c_string(idna_input), unicode_to_c_ustring(unicode_output),
         'kSafe' if unicode_allowed else 'kUnsafe'))


def main(args=None):
  if args is None:
    args = sys.argv[1:]

  parser = argparse.ArgumentParser(description='Generate an IDN test case.')
  parser.add_argument('domain',
                      metavar='DOMAIN',
                      nargs='?',
                      help='the Unicode domain (not encoded)')
  parser.add_argument('--name',
                      metavar='NAME',
                      help='the name of the test case')
  parser.add_argument('--no-unicode',
                      action='store_false',
                      dest='unicode_allowed',
                      default=True,
                      help='expect the domain to be Punycoded')
  parser.add_argument('--test',
                      action='store_true',
                      dest='run_tests',
                      help='run unit tests')

  args = parser.parse_args(args)

  if args.run_tests:
    import doctest
    doctest.testmod()
    return

  if not args.domain:
    parser.error('Required argument: DOMAIN')

  if '://' in args.domain:
    parser.error('A URL must not be passed as the domain argument')

  make_case(args.domain,
            unicode_allowed=args.unicode_allowed,
            case_name=args.name)


if __name__ == '__main__':
  sys.exit(main())
