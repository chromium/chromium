#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import sys

import deb_version

class PackageVersionIntervalEndpoint:
  def __init__(self, is_open, is_inclusive, version):
    self._is_open = is_open;
    self._is_inclusive = is_inclusive
    self._version = version

  def _intersect(self, other, is_start):
    if self._is_open and other._is_open:
      return self
    if self._is_open:
      return other
    if other._is_open:
      return self
    cmp_code = self._version.__cmp__(other._version)
    if not is_start:
      cmp_code *= -1
    if cmp_code > 0:
      return self
    if cmp_code < 0:
      return other
    if not self._is_inclusive:
      return self
    return other

  def __str__(self):
    return 'PackageVersionIntervalEndpoint(%s, %s, %s)' % (
        self._is_open, self._is_inclusive, self._version)

  def __eq__(self, other):
    if self._is_open and other._is_open:
      return True
    return (self._is_open == other._is_open and
            self._is_inclusive == other._is_inclusive and
            self._version == other._version)


class PackageVersionInterval:
  def __init__(self, string_rep, package, start, end):
    self.string_rep = string_rep
    self.package = package
    self.start = start
    self.end = end

  def contains(self, version):
    if not self.start._is_open:
      if self.start._is_inclusive:
        if version < self.start._version:
          return False
      elif version <= self.start._version:
        return False
    if not self.end._is_open:
      if self.end._is_inclusive:
        if version > self.end._version:
          return False
      elif version >= self.end._version:
        return False
    return True

  def intersect(self, other):
    return PackageVersionInterval(
        '', '', self.start._intersect(other.start, True),
        self.end._intersect(other.end, False))

  def implies(self, other):
    if self.package != other.package:
      return False
    return self.intersect(other) == self

  def __str__(self):
    return 'PackageVersionInterval(%s)' % self.string_rep

  def __eq__(self, other):
    return self.start == other.start and self.end == other.end


class PackageVersionIntervalSet:
  def __init__(self, intervals):
    self.intervals = intervals

  def formatted(self):
    return ' | '.join([interval.string_rep for interval in self.intervals])

  def _interval_implies_other_intervals(self, interval, other_intervals):
    for other_interval in other_intervals:
      if interval.implies(other_interval):
        return True
    return False

  def implies(self, other):
    # This disjunction implies |other| if every term in this
    # disjunction implies some term in |other|.
    for interval in self.intervals:
      if not self._interval_implies_other_intervals(interval, other.intervals):
        return False
    return True


def version_interval_endpoints_from_exp(op, version):
  open_endpoint = PackageVersionIntervalEndpoint(True, None, None)
  inclusive_endpoint = PackageVersionIntervalEndpoint(False, True, version)
  exclusive_endpoint = PackageVersionIntervalEndpoint(False, False, version)
  if op == '>=':
    return (inclusive_endpoint, open_endpoint)
  if op == '<=':
    return (open_endpoint, inclusive_endpoint)
  if op == '>>' or op == '>':
    return (exclusive_endpoint, open_endpoint)
  if op == '<<' or op == '<':
    return (open_endpoing, exclusive_endpoint)
  assert op == '='
  return (inclusive_endpoint, inclusive_endpoint)


def parse_dep(dep):
  """Parses a package and version requirement formatted by dpkg-shlibdeps.

  Args:
      dep: A string of the format "package (op version)"

  Returns:
      A PackageVersionInterval.
  """
  package_name_regex = r'[a-z][a-z0-9\+\-\.]+'
  match = re.match('^(%s)$' % package_name_regex, dep)
  if match:
    return PackageVersionInterval(dep, match.group(1),
        PackageVersionIntervalEndpoint(True, None, None),
        PackageVersionIntervalEndpoint(True, None, None))
  match = re.match(r'^(%s) \(([\>\=\<]+) ([\~0-9A-Za-z\+\-\.\:]+)\)$' %
                   package_name_regex, dep)
  if match:
    (start, end) = version_interval_endpoints_from_exp(
        match.group(2), deb_version.DebVersion(match.group(3)))
    return PackageVersionInterval(dep, match.group(1), start, end)
  print >> sys.stderr, 'Failed to parse ' + dep
  sys.exit(1)


def parse_interval_set(deps):
  r"""Parses a disjunction of package version requirements.

  Args:
      deps: A string of the format
          "package \(op version\) (| package \(op version\))*"

  Returns:
      A list of PackageVersionIntervals
  """
  return PackageVersionIntervalSet(
      [parse_dep(dep.strip()) for dep in deps.split('|')])
