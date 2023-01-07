#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for writers.admx_writer."""

import re
import unittest


class XmlWriterBaseTest(unittest.TestCase):
  '''Base class for XML writer unit-tests.
  '''

  def GetXMLOfChildren(self, parent):
    '''Returns the XML of all child nodes of the given parent node.
    Args:
      parent: The XML of the children of this node will  be returned.

    Return: XML of the chrildren of the parent node.
    '''
    raw_pretty_xml = ''.join(
        child.toprettyxml(indent='  ') for child in parent.childNodes)
    # Python 2.6.5 which is present in Lucid has bug in its pretty print
    # function which produces new lines around string literals. This has been
    # fixed in Precise which has Python 2.7.3 but we have to keep compatibility
    # with both for now.
    text_re = re.compile('>\n\s+([^<>\s].*?)\n\s*</', re.DOTALL)
    return text_re.sub('>\g<1></', raw_pretty_xml)

  def AssertXMLEquals(self, output, expected_output):
    '''Asserts if the passed XML arguements are equal.
    Args:
      output: Actual XML text.
      expected_output: Expected XML text.
    '''
    self.assertEquals(output.strip(), expected_output.strip())
