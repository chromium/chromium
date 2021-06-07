#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import generate_policy_source


class CppGenerationTest(unittest.TestCase):

  def testDefaultValueGeneration(self):
    """Tests generation of default policy values."""
    # Bools
    stmts, expr = generate_policy_source._GenerateDefaultValue(True)
    self.assertListEqual([], stmts)
    self.assertEqual('std::make_unique<base::Value>(true)', expr)
    stmts, expr = generate_policy_source._GenerateDefaultValue(False)
    self.assertListEqual([], stmts)
    self.assertEqual('std::make_unique<base::Value>(false)', expr)

    # Ints
    stmts, expr = generate_policy_source._GenerateDefaultValue(33)
    self.assertListEqual([], stmts)
    self.assertEqual('std::make_unique<base::Value>(33)', expr)

    # Strings
    stmts, expr = generate_policy_source._GenerateDefaultValue('foo')
    self.assertListEqual([], stmts)
    self.assertEqual('std::make_unique<base::Value>("foo")', expr)

    # Empty list
    stmts, expr = generate_policy_source._GenerateDefaultValue([])
    self.assertListEqual(
        ['auto default_value = std::make_unique<base::ListValue>();'], stmts)
    self.assertEqual('std::move(default_value)', expr)

    # List with values
    stmts, expr = generate_policy_source._GenerateDefaultValue([1, '2'])
    self.assertListEqual([
        'auto default_value = std::make_unique<base::ListValue>();',
        'default_value->Append(std::make_unique<base::Value>(1));',
        'default_value->Append(std::make_unique<base::Value>("2"));'
    ], stmts)
    self.assertEqual('std::move(default_value)', expr)

    # Recursive lists are not supported.
    stmts, expr = generate_policy_source._GenerateDefaultValue([1, []])
    self.assertListEqual([], stmts)
    self.assertIsNone(expr)

    # Arbitary types are not supported.
    stmts, expr = generate_policy_source._GenerateDefaultValue(object())
    self.assertListEqual([], stmts)
    self.assertIsNone(expr)


if __name__ == '__main__':
  unittest.main()
