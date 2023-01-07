#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import deb_version
import package_version_interval

def make_interval(start_open, start_inclusive, start_cmp,
                  end_open, end_inclusive, end_cmp, dep='', package=''):
  start = package_version_interval.PackageVersionIntervalEndpoint(
      start_open, start_inclusive, start_cmp)
  end = package_version_interval.PackageVersionIntervalEndpoint(
      end_open, end_inclusive, end_cmp)
  return package_version_interval.PackageVersionInterval(
      dep, package, start, end)


# PackageVersionInterval.intersect() test.
assert (make_interval(True, None, None, False, True, 10).intersect(
    make_interval(False, True, 5, True, None, None)) ==
      make_interval(False, True, 5, False, True, 10))
assert (make_interval(False, True, 3, False, True, 7).intersect(
    make_interval(False, True, 4, False, True, 6)) ==
      make_interval(False, True, 4, False, True, 6))
assert (make_interval(False, False, 3, False, False, 7).intersect(
    make_interval(False, True, 3, False, True, 7)) ==
      make_interval(False, False, 3, False, False, 7))

# PackageVersionInterval.contains() test.
assert make_interval(False, False, 3, False, False, 7).contains(5)
assert not make_interval(False, False, 3, False, False, 7).contains(3)
assert not make_interval(False, False, 3, False, False, 7).contains(7)
assert make_interval(False, True, 3, False, True, 7).contains(3)
assert make_interval(False, True, 3, False, True, 7).contains(7)
assert make_interval(True, None, None, False, True, 7).contains(5)
assert make_interval(False, True, 3, True, None, None).contains(5)
assert not make_interval(True, None, None, False, True, 7).contains(8)
assert not make_interval(False, True, 3, True, None, None).contains(2)

# parse_dep() test.
assert (package_version_interval.parse_dep('libfoo (> 1.0)') ==
        make_interval(False, False, deb_version.DebVersion('1.0'),
                                 True, None, None, package='libfoo'))
assert (package_version_interval.parse_dep('libbar (>> a.b.c)') ==
        make_interval(False, False, deb_version.DebVersion('a.b.c'),
                                 True, None, None, package='libbar'))
assert (package_version_interval.parse_dep('libbaz (= 2:1.2.3-1)') ==
        make_interval(
            False, True, deb_version.DebVersion('2:1.2.3-1'),
            False, True, deb_version.DebVersion('2:1.2.3-1'), package='libbaz'))

# PackageVersionInterval.implies() test.
assert package_version_interval.parse_dep('libfoo').implies(
    package_version_interval.parse_dep('libfoo'))
assert package_version_interval.parse_dep('libfoo (>> 2)').implies(
    package_version_interval.parse_dep('libfoo (>> 1)'))
assert not package_version_interval.parse_dep('libfoo (>> 1)').implies(
    package_version_interval.parse_dep('libfoo (>> 2)'))
assert package_version_interval.parse_dep('libfoo (>> 1)').implies(
    package_version_interval.parse_dep('libfoo (>= 1)'))
assert not package_version_interval.parse_dep('libfoo (>= 1)').implies(
    package_version_interval.parse_dep('libfoo (>> 1)'))
assert package_version_interval.parse_dep('libfoo (= 10)').implies(
    package_version_interval.parse_dep('libfoo (>= 1)'))
assert not package_version_interval.parse_dep('libfoo (>= 1)').implies(
    package_version_interval.parse_dep('libfoo (= 10)'))
assert package_version_interval.parse_dep('libfoo (= 10)').implies(
    package_version_interval.parse_dep('libfoo (>> 1)'))
assert not package_version_interval.parse_dep('libfoo (>> 1)').implies(
    package_version_interval.parse_dep('libfoo (= 10)'))
assert package_version_interval.parse_dep('libfoo (= 1)').implies(
    package_version_interval.parse_dep('libfoo (>= 1)'))
assert not package_version_interval.parse_dep('libfoo (>= 1)').implies(
    package_version_interval.parse_dep('libfoo (= 1)'))
assert not package_version_interval.parse_dep('libfoo (= 1)').implies(
    package_version_interval.parse_dep('libfoo (>> 1)'))
assert not package_version_interval.parse_dep('libfoo (>> 1)').implies(
    package_version_interval.parse_dep('libfoo (= 1)'))

# PackageVersionIntervalSet.implies() test.
assert (package_version_interval.parse_interval_set('libfoo | libbar').implies(
    package_version_interval.parse_interval_set('libfoo | libbar')))
assert (package_version_interval.parse_interval_set('libfoo').implies(
    package_version_interval.parse_interval_set('libfoo | libbar')))
assert not (
    package_version_interval.parse_interval_set('libfoo | libbar').implies(
    package_version_interval.parse_interval_set('libfoo')))
assert (package_version_interval.parse_interval_set('libbar').implies(
    package_version_interval.parse_interval_set('libfoo | libbar')))
assert not (
    package_version_interval.parse_interval_set('libfoo | libbar').implies(
    package_version_interval.parse_interval_set('libbar')))
assert (package_version_interval.parse_interval_set(
    'libfoo (>> 2) | libbar (>> 2)').implies(
    package_version_interval.parse_interval_set(
        'libfoo (>> 1) | libbar (>> 1)')))
