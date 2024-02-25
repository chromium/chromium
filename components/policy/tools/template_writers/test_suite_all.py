#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Unit test suite that collects template_writer tests.'''

import os
import sys
import unittest


class TestSuiteAll(unittest.TestSuite):

  def __init__(self):
    super(TestSuiteAll, self).__init__()
    # Imports placed here to prevent circular imports.
    # pylint: disable-msg=C6204
    import policy_template_generator_unittest
    import writers.adm_writer_unittest
    import writers.adml_writer_unittest
    import writers.admx_writer_unittest
    import writers.android_policy_writer_unittest
    import writers.doc_writer_unittest
    import writers.google_adml_writer_unittest
    import writers.google_admx_writer_unittest
    import writers.ios_app_config_writer_unittest
    import writers.jamf_writer_unittest
    import writers.json_writer_unittest
    import writers.plist_strings_writer_unittest
    import writers.plist_writer_unittest
    import writers.reg_writer_unittest
    import writers.template_writer_unittest
    import writers.xml_writer_base_unittest

    test_classes = [
        policy_template_generator_unittest.PolicyTemplateGeneratorUnittest,
        writers.adm_writer_unittest.AdmWriterUnittest,
        writers.adml_writer_unittest.AdmlWriterUnittest,
        writers.admx_writer_unittest.AdmxWriterUnittest,
        writers.android_policy_writer_unittest.AndroidPolicyWriterUnittest,
        writers.doc_writer_unittest.DocWriterUnittest,
        writers.google_adml_writer_unittest.GoogleAdmlWriterUnittest,
        writers.google_admx_writer_unittest.GoogleAdmxWriterUnittest,
        writers.ios_app_config_writer_unittest.IOSAppConfigWriterUnitTests,
        writers.jamf_writer_unittest.JamfWriterUnitTests,
        writers.json_writer_unittest.JsonWriterUnittest,
        writers.plist_strings_writer_unittest.PListStringsWriterUnittest,
        writers.plist_writer_unittest.PListWriterUnittest,
        writers.reg_writer_unittest.RegWriterUnittest,
        writers.template_writer_unittest.TemplateWriterUnittests,
        writers.xml_writer_base_unittest.XmlWriterBaseTest,
        # add test classes here, in alphabetical order...
    ]

    for test_class in test_classes:
      self.addTest(unittest.makeSuite(test_class))


if __name__ == '__main__':
  test_result = unittest.TextTestRunner(verbosity=2).run(TestSuiteAll())
  sys.exit(len(test_result.errors) + len(test_result.failures))
