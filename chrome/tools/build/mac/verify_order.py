# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Verifies that no global text symbols are present in a Mach-O file
# that is not in a given allowlist. Also verifies the order of global symbols
# matches the order in the allowlist.

from __future__ import print_function

import argparse
import os.path
import re
import sys
import subprocess


def has_duplicates(x):
  return len(x) != len(set(x))


def parse_order_file(filename):
  """Parses a file containing symbols like passed to ld64's -order_file.

  Doesn't support arch or object file prefixes.
  """
  strip_comments = re.compile('#.*$')
  symbols = [strip_comments.sub('', line).strip() for line in open(filename)]
  symbols = list(filter(None, symbols))
  if has_duplicates(symbols):
    print('order file "%s" contains duplicate symbols' % filename,
          file=sys.stderr)
    sys.exit(1)
  return symbols


def check_symbol_table(binary, allowed_symbols, nm, symbol_file):
  actual_symbols = subprocess.check_output([nm, '-jUng', binary]).decode('utf8')
  actual_symbols = [s.rstrip() for s in actual_symbols.splitlines()]
  def print_syms(syms):
    print('\n'.join(['  ' + s for s in syms]), file=sys.stderr)
  try:
    # Check that actual_symbols is a sublist of allowed_symbols.
    # Every exported symbol must be in allowed_symbols, and the order of the
    # exported symbols must match the order in allowed_symbols. Not all
    # symbols in allowed_symbols need to be present in the actual exports.
    # The approach here is O(m*n), but allowed_symbols is very small so that's
    # fine. Order matters, so can't use set().
    symbol_indices = [allowed_symbols.index(s) for s in actual_symbols]
    if symbol_indices != sorted(symbol_indices):
      print('"%s" exports symbols in order different from order file %s' %
            (binary, symbol_file),
            file=sys.stderr)
      print('actual order:', file=sys.stderr)
      print_syms(actual_symbols)
      print('expected order:', file=sys.stderr)
      print_syms(allowed_symbols)
      sys.exit(1)
    # If we get here, every symbol in actual_symbols is in allowed_symbols, in
    # the right order. nm's output doesn't contain duplicates, so we are done.
    assert not has_duplicates(allowed_symbols)
  except ValueError as e:
    print('unexpected export: %s' % e, file=sys.stderr)
    print('allowed exports from %s:' % symbol_file, file=sys.stderr)
    print_syms(allowed_symbols)
    sys.exit(1)


def main():
  parser = argparse.ArgumentParser(
    description='Check order of exported symbols of a given binary.')
  parser.add_argument('--stamp', required=True,
    help='Touch this stamp file on success.')
  parser.add_argument('--binary', required=True, help='Check this binary.')
  parser.add_argument('--nm-path', required=True, help='Path to nm')
  parser.add_argument('--symbol-file', required=True,
    help='Order file listing expected public symbols.')
  args = parser.parse_args()

  allowed_symbols = parse_order_file(args.symbol_file)
  check_symbol_table(
      args.binary, allowed_symbols, args.nm_path, args.symbol_file)
  # TODO(thakis): Also verify global symbols in the binary's export trie.

  open(args.stamp, 'w').close()  # Update mtime on stamp file.


if __name__ == '__main__':
  main()
