#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Common tools for unit-testing writers.'''

import unittest
import policy_template_generator
import template_formatter
import textwrap
import writer_configuration


class WriterUnittestCommon(unittest.TestCase):
  '''Common class for unittesting writers.'''

  def GetOutput(self, policy_json, definitions, writer_type):
    '''Generates an output of a writer.

    Args:
      policy_json: Raw policy JSON string.
      definitions: Definitions to create writer configurations.
      writer_type: Writer type (e.g. 'admx'), see template_formatter.py.

    Returns:
      The string of the template created by the writer.
    '''

    # Evaluate policy_json. For convenience, fix indentation in statements like
    # policy_json = '''
    #   {
    #     ...
    #   }''')
    start_idx = 1 if policy_json[0] == '\n' else 0
    policy_data = eval(textwrap.dedent(policy_json[start_idx:]))

    config = writer_configuration.GetConfigurationForBuild(definitions)
    policy_generator = \
        policy_template_generator.PolicyTemplateGenerator(config, policy_data)
    writer = template_formatter.GetWriter(writer_type, config)
    return policy_generator.GetTemplateText(writer)
