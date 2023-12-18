#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Unit tests for writers.doc_writer'''

import json
import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../../../..'))

import unittest
from xml.dom import minidom

from writers import writer_unittest_common
from writers import doc_writer


class MockMessageDictionary:
  '''A mock dictionary passed to a writer as the dictionary of
  localized messages.
  '''

  # Dictionary of messages.
  msg_dict = {}


class DocWriterUnittest(writer_unittest_common.WriterUnittestCommon):
  '''Unit tests for DocWriter.'''

  def setUp(self):
    # Create a writer for the tests.
    self.writer = doc_writer.GetWriter(
        config={
            'app_name': 'Chrome',
            'frame_name': 'Chrome Frame',
            'os_name': 'ChromeOS',
            'webview_name': 'WebView',
            'android_webview_restriction_prefix': 'mock.prefix:',
            'win_config': {
                'win': {
                    'reg_mandatory_key_name': 'MockKey',
                    'reg_recommended_key_name': 'MockKeyRec',
                },
                'chrome_os': {
                    'reg_mandatory_key_name': 'MockKeyCrOS',
                    'reg_recommended_key_name': 'MockKeyCrOSRec',
                },
            },
            'build': 'test_product',
        })
    self.writer.messages = {
        'doc_back_to_top': {
            'text': '_test_back_to_top'
        },
        'doc_complex_policies_on_windows': {
            'text': '_test_complex_policies_win'
        },
        'doc_data_type': {
            'text': '_test_data_type'
        },
        'doc_description': {
            'text': '_test_description'
        },
        'doc_schema': {
            'text': '_test_schema'
        },
        'doc_url_schema': {
            'text': '_test_url_schema'
        },
        'doc_arc_support': {
            'text': '_test_arc_support'
        },
        'doc_description_column_title': {
            'text': '_test_description_column_title'
        },
        'doc_example_value': {
            'text': '_test_example_value'
        },
        'doc_win_example_value': {
            'text': '_test_example_value_win'
        },
        'doc_feature_dynamic_refresh': {
            'text': '_test_feature_dynamic_refresh'
        },
        'doc_feature_can_be_recommended': {
            'text': '_test_feature_recommended'
        },
        'doc_feature_can_be_mandatory': {
            'text': '_test_feature_mandatory'
        },
        'doc_banner': {
            'text': '_test_banner'
        },
        'doc_intro': {
            'text': '_test_intro'
        },
        'doc_mac_linux_pref_name': {
            'text': '_test_mac_linux_pref_name'
        },
        'doc_android_restriction_name': {
            'text': '_test_android_restriction_name'
        },
        'doc_android_webview_restriction_name': {
            'text': '_test_android_webview_restriction_name'
        },
        'doc_note': {
            'text': '_test_note'
        },
        'doc_name_column_title': {
            'text': '_test_name_column_title'
        },
        'doc_not_supported': {
            'text': '_test_not_supported'
        },
        'doc_since_version': {
            'text': '..$6..'
        },
        'doc_supported': {
            'text': '_test_supported'
        },
        'doc_supported_features': {
            'text': '_test_supported_features'
        },
        'doc_supported_on': {
            'text': '_test_supported_on'
        },
        'doc_win_reg_loc': {
            'text': '_test_win_reg_loc'
        },
        'doc_oma_uri': {
            'text': '_test_oma_uri'
        },
        'doc_bla': {
            'text': '_test_bla'
        },
        'doc_policy_atomic_group': {
            'text': '_test_policy_atomic_group'
        },
        'doc_policy_in_atomic_group': {
            'text': '_test_policy_in_atomic_group'
        }
    }
    self.writer.Init()

    # It is not worth testing the exact content of style attributes.
    # Therefore we override them here with shorter texts.
    for key in self.writer._STYLE.keys():
      self.writer._STYLE[key] = 'style_%s;' % key
    # Add some more style attributes for additional testing.
    self.writer._STYLE['key1'] = 'style1;'
    self.writer._STYLE['key2'] = 'style2;'

    # Create a DOM document for the tests.
    dom_impl = minidom.getDOMImplementation('')
    self.doc = dom_impl.createDocument(None, 'root', None)
    self.doc_root = self.doc.documentElement

  def testSkeleton(self):
    # Test if DocWriter creates the skeleton of the document correctly.
    self.writer.BeginTemplate()
    self.assertEquals(
        self.writer._main_div.toxml(), '<div>'
        '<div style="style_div.banner;"><p>_test_banner</p></div>'
        '<div>'
        '<a name="top"/><br/><p>_test_intro</p><br/><br/><br/>'
        '<table style="style_table;">'
        '<thead><tr style="style_tr;">'
        '<td style="style_td;style_td.left;style_thead td;">'
        '_test_name_column_title'
        '</td>'
        '<td style="style_td;style_td.right;style_thead td;">'
        '_test_description_column_title'
        '</td>'
        '</tr></thead>'
        '<tbody/>'
        '</table>'
        '</div>'
        '<div/>'
        '</div>')

  def testVersionAnnotation(self):
    # Test if DocWriter creates the skeleton of the document correctly.
    self.writer.config['version'] = '39.0.0.0'
    self.writer.BeginTemplate()
    self.assertEquals(
        self.writer._main_div.toxml(), '<div>'
        '<!--test_product version: 39.0.0.0-->'
        '<div style="style_div.banner;"><p>_test_banner</p></div>'
        '<div>'
        '<a name="top"/><br/><p>_test_intro</p><br/><br/><br/>'
        '<table style="style_table;">'
        '<thead><tr style="style_tr;">'
        '<td style="style_td;style_td.left;style_thead td;">'
        '_test_name_column_title'
        '</td>'
        '<td style="style_td;style_td.right;style_thead td;">'
        '_test_description_column_title'
        '</td>'
        '</tr></thead>'
        '<tbody/>'
        '</table>'
        '</div>'
        '<div/>'
        '</div>')

  def testGetLocalizedMessage(self):
    # Test if localized messages are retrieved correctly.
    self.writer.messages = {'doc_hello_world': {'text': 'hello, vilag!'}}
    self.assertEquals(
        self.writer.GetLocalizedMessage('hello_world'), 'hello, vilag!')

  def testAddStyledElement(self):
    # Test function DocWriter.AddStyledElement()

    # Test the case of zero style.
    e1 = self.writer._AddStyledElement(self.doc_root, 'z', [], {'a': 'b'},
                                       'text')
    self.assertEquals(e1.toxml(), '<z a="b">text</z>')

    # Test the case of one style.
    e2 = self.writer._AddStyledElement(self.doc_root, 'z', ['key1'], {'a': 'b'},
                                       'text')
    self.assertEquals(e2.toxml(), '<z a="b" style="style1;">text</z>')

    # Test the case of two styles.
    e3 = self.writer._AddStyledElement(self.doc_root, 'z', ['key1', 'key2'],
                                       {'a': 'b'}, 'text')
    self.assertEquals(e3.toxml(), '<z a="b" style="style1;style2;">text</z>')

  def testAddDescriptionIntEnum(self):
    # Test if URLs are replaced and choices of 'int-enum' policies are listed
    # correctly.
    policy = {
        'type':
            'int-enum',
        'items': [
            {
                'value': 0,
                'caption': 'Disable foo'
            },
            {
                'value': 2,
                'caption': 'Solve your problem'
            },
            {
                'value': 5,
                'caption': 'Enable bar'
            },
        ],
        'desc':
            '''This policy disables foo, except in case of bar.
See http://policy-explanation.example.com for more details.
'''
    }
    self.writer._AddDescription(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(),
        '''<root><p>This policy disables foo, except in case of bar.
See <a href="http://policy-explanation.example.com">http://policy-explanation.example.com</a> for more details.
</p><ul><li>0 = Disable foo</li><li>2 = Solve your problem</li><li>5 = Enable bar</li></ul></root>'''
    )

  def testAddDescriptionStringEnum(self):
    # Test if URLs are replaced and choices of 'int-enum' policies are listed
    # correctly.
    policy = {
        'type':
            'string-enum',
        'items': [
            {
                'value': "one",
                'caption': 'Disable foo'
            },
            {
                'value': "two",
                'caption': 'Solve your problem'
            },
            {
                'value': "three",
                'caption': 'Enable bar'
            },
        ],
        'desc':
            '''This policy disables foo, except in case of bar.
See http://policy-explanation.example.com for more details.
'''
    }
    self.writer._AddDescription(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(),
        '''<root><p>This policy disables foo, except in case of bar.
See <a href="http://policy-explanation.example.com">http://policy-explanation.example.com</a> for more details.
</p><ul><li>&quot;one&quot; = Disable foo</li><li>&quot;two&quot; = Solve your problem</li><li>&quot;three&quot; = Enable bar</li></ul></root>'''
    )

  def testAddSchema(self):
    # Test if the schema of a policy is handled correctly.
    policy = {
        'type': 'dict',
        'schema': {
            'properties': {
                'foo': {
                    'type': 'integer'
                }
            },
            'type': 'object'
        }
    }
    self.writer._AddSchema(self.doc_root, policy['schema'])
    self.assertEquals(
        self.doc_root.toxml(), '<root>'
        '<dt style="style_dt;">_test_schema</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">{\n'
        '  &quot;properties&quot;: {\n'
        '    &quot;foo&quot;: {\n'
        '      &quot;type&quot;: &quot;integer&quot;\n'
        '    }\n'
        '  }, \n'
        '  &quot;type&quot;: &quot;object&quot;\n'
        '}</dd></root>')

  def testAddUrlSchema(self):
    # Test if the expanded schema description of a policy is handled correctly.
    policy = {'url_schema': 'https://example.com/details'}
    self.writer._AddTextWithLinks(self.doc_root, policy['url_schema'])
    self.assertEquals(
        self.doc_root.toxml(),
        '<root><a href="https://example.com/details">https://example.com/details</a></root>'
    )

  def testAddFeatures(self):
    # Test if the list of features of a policy is handled correctly.
    policy = {
        'features': {
            'spaceship_docking': False,
            'dynamic_refresh': True,
            'can_be_recommended': True,
        }
    }
    self.writer._FEATURE_MAP = {
        'can_be_recommended': 'Can Be Recommended',
        'dynamic_refresh': 'Dynamic Refresh',
        'spaceship_docking': 'Spaceship Docking',
    }
    self.writer._AddFeatures(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root>'
        'Can Be Recommended: _test_supported, '
        'Dynamic Refresh: _test_supported, '
        'Spaceship Docking: _test_not_supported'
        '</root>')

  def testAddListExample(self):
    policy = {
        'name':
        'PolicyName',
        'example_value': ['Foo', 'Bar'],
        'supported_on': [{
            'platform': 'win'
        }, {
            'platform': 'mac'
        }, {
            'platform': 'linux'
        }, {
            'platform': 'chrome_os'
        }]
    }
    self.writer._AddListExample(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root>'
        '<dl style="style_dd dl;">'
        '<dt>_test_example_value_win</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">'
        'MockKey\\PolicyName\\1 = &quot;Foo&quot;\n'
        'MockKey\\PolicyName\\2 = &quot;Bar&quot;'
        '</dd>'
        '<dt>Android/Linux:</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">'
        '[\n'
        '  &quot;Foo&quot;,\n'
        '  &quot;Bar&quot;\n'
        ']'
        '</dd>'
        '<dt>Mac:</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">'
        '&lt;array&gt;\n'
        '  &lt;string&gt;Foo&lt;/string&gt;\n'
        '  &lt;string&gt;Bar&lt;/string&gt;\n'
        '&lt;/array&gt;'
        '</dd>'
        '</dl>'
        '</root>')

  def testBoolExample(self):
    # Test representation of boolean example values.
    policy = {
        'name':
        'PolicyName',
        'type':
        'main',
        'example_value':
        True,
        'supported_on': [{
            'platform': 'win'
        }, {
            'platform': 'mac'
        }, {
            'platform': 'linux'
        }, {
            'platform': 'android'
        }]
    }
    e1 = self.writer.AddElement(self.doc_root, 'e1')
    self.writer._AddExample(e1, policy)
    self.assertEquals(
        e1.toxml(), '<e1>0x00000001 (Windows),'
        ' true (Linux), true (Android),'
        ' &lt;true /&gt; (Mac)'
        '<dl><dt>Windows (Intune):</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;enabled/&gt;</dd></dl>'
        '</e1>')

    policy = {
        'name':
        'PolicyName',
        'type':
        'main',
        'example_value':
        False,
        'supported_on': [{
            'platform': 'win'
        }, {
            'platform': 'mac'
        }, {
            'platform': 'linux'
        }, {
            'platform': 'android'
        }]
    }
    e2 = self.writer.AddElement(self.doc_root, 'e2')
    self.writer._AddExample(e2, policy)
    self.assertEquals(
        e2.toxml(), '<e2>0x00000000 (Windows),'
        ' false (Linux), false (Android),'
        ' &lt;false /&gt; (Mac)'
        '<dl><dt>Windows (Intune):</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;disabled/&gt;</dd></dl>'
        '</e2>')

  def testIntEnumExample(self):
    # Test representation of 'int-enum' example values.
    policy = {
        'name':
        'PolicyName',
        'type':
        'int-enum',
        'example_value':
        16,
        'supported_on': [{
            'platform': 'win'
        }, {
            'platform': 'mac'
        }, {
            'platform': 'linux'
        }, {
            'platform': 'android'
        }]
    }
    self.writer._AddExample(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(),
        '<root>0x00000010 (Windows), 16 (Linux), 16 (Android), 16 (Mac)'
        '<dl><dt>Windows (Intune):</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;enabled/&gt;</dd>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;data id=&quot;PolicyName&quot; value=&quot;16&quot;/&gt;</dd></dl>'
        '</root>')

  def testStringEnumExample(self):
    # Test representation of 'string-enum' example values.
    policy = {
        'name': 'PolicyName',
        'type': 'string-enum',
        'example_value': "wacky",
        'supported_on': []
    }
    self.writer._AddExample(self.doc_root, policy)
    self.assertEquals(self.doc_root.toxml(), '<root>&quot;wacky&quot;</root>')

  def testListExample(self):
    # Test representation of 'list' example values.
    policy = {
        'name': 'PolicyName',
        'type': 'list',
        'example_value': ['one', 'two'],
        'supported_on': [{
            'platform': 'linux'
        }]
    }
    self.writer._AddExample(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root><dl style="style_dd dl;">'
        '<dt>Android/Linux:</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">'
        '[\n'
        '  &quot;one&quot;,\n'
        '  &quot;two&quot;\n'
        ']'
        '</dd></dl></root>')

  def testStringEnumListExample(self):
    # Test representation of 'string-enum-list' example values.
    policy = {
        'name': 'PolicyName',
        'type': 'string-enum-list',
        'example_value': ['one', 'two'],
        'supported_on': [{
            'platform': 'linux'
        }]
    }
    self.writer._AddExample(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root><dl style="style_dd dl;">'
        '<dt>Android/Linux:</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">'
        '[\n'
        '  &quot;one&quot;,\n'
        '  &quot;two&quot;\n'
        ']'
        '</dd></dl></root>')

  def testStringExample(self):
    # Test representation of 'string' example values.
    policy = {
        'name': 'PolicyName',
        'type': 'string',
        'example_value': 'awesome-example',
        'supported_on': []
    }
    self.writer._AddExample(self.doc_root, policy)
    self.assertEquals(self.doc_root.toxml(),
                      '<root>&quot;awesome-example&quot;</root>')

  def testIntExample(self):
    # Test representation of 'int' example values.
    policy = {
        'name':
        'PolicyName',
        'type':
        'int',
        'example_value':
        26,
        'supported_on': [{
            'platform': 'win'
        }, {
            'platform': 'mac'
        }, {
            'platform': 'linux'
        }, {
            'platform': 'android'
        }]
    }
    self.writer._AddExample(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(),
        '<root>0x0000001a (Windows), 26 (Linux), 26 (Android), 26 (Mac)'
        '<dl><dt>Windows (Intune):</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;enabled/&gt;</dd>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;data id=&quot;PolicyName&quot; value=&quot;26&quot;/&gt;</dd></dl>'
        '</root>')

  def testAddPolicyAttribute(self):
    # Test creating a policy attribute term-definition pair.
    self.writer._AddPolicyAttribute(self.doc_root, 'bla', 'hello, world',
                                    ['key1'])
    self.assertEquals(
        self.doc_root.toxml(), '<root>'
        '<dt style="style_dt;">_test_bla</dt>'
        '<dd style="style1;">hello, world</dd>'
        '</root>')

  def testAddPolicyDetails(self):
    # Test if the definition list (<dl>) of policy details is created correctly.
    policy = {
        'type':
        'main',
        'name':
        'TestPolicyName',
        'caption':
        'TestPolicyCaption',
        'desc':
        'TestPolicyDesc',
        'supported_on': [{
            'product': 'chrome',
            'platform': 'win',
            'since_version': '8',
            'until_version': '',
        },
                         {
                             'product': 'chrome',
                             'platform': 'mac',
                             'since_version': '8',
                             'until_version': '',
                         },
                         {
                             'product': 'chrome',
                             'platform': 'linux',
                             'since_version': '8',
                             'until_version': '',
                         },
                         {
                             'product': 'chrome',
                             'platform': 'android',
                             'since_version': '30',
                             'until_version': '',
                         },
                         {
                             'product': 'webview',
                             'platform': 'android',
                             'since_version': '47',
                             'until_version': '',
                         },
                         {
                             'product': 'chrome',
                             'platform': 'chrome_os',
                             'since_version': '55',
                             'until_version': '',
                         }],
        'features': {
            'dynamic_refresh': False
        },
        'example_value':
        False,
        'arc_support':
        'TestArcSupportNote'
    }
    self.writer._AddPolicyDetails(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root><dl>'
        '<dt style="style_dt;">_test_data_type</dt>'
        '<dd>Boolean [Windows:REG_DWORD]</dd>'
        '<dt style="style_dt;">_test_win_reg_loc</dt>'
        '<dd style="style_.monospace;">MockKey\TestPolicyName</dd>'
        '<dt style="style_dt;">_test_oma_uri</dt>'
        '<dd style="style_.monospace;">.\\Device\\Vendor\\MSFT\\Policy\\Config\\Chrome~Policy~chromium\\TestPolicyName</dd>'
        '<dt style="style_dt;">_test_mac_linux_pref_name</dt>'
        '<dd style="style_.monospace;">TestPolicyName</dd>'
        '<dt style="style_dt;">_test_android_restriction_name</dt>'
        '<dd style="style_.monospace;">TestPolicyName</dd>'
        '<dt style="style_dt;">_test_android_webview_restriction_name</dt>'
        '<dd style="style_.monospace;">mock.prefix:TestPolicyName</dd>'
        '<dt style="style_dt;">_test_supported_on</dt>'
        '<dd>'
        '<ul style="style_ul;">'
        '<li>Chrome (Windows) ..8..</li>'
        '<li>Chrome (Mac) ..8..</li>'
        '<li>Chrome (Linux) ..8..</li>'
        '<li>Chrome (Android) ..30..</li>'
        '<li>WebView (Android) ..47..</li>'
        '<li>Chrome (ChromeOS) ..55..</li>'
        '</ul>'
        '</dd>'
        '<dt style="style_dt;">_test_supported_features</dt>'
        '<dd>_test_feature_dynamic_refresh: _test_not_supported</dd>'
        '<dt style="style_dt;">_test_description</dt><dd><p>TestPolicyDesc</p></dd>'
        '<dt style="style_dt;">_test_arc_support</dt>'
        '<dd><p>TestArcSupportNote</p></dd>'
        '<dt style="style_dt;">_test_example_value</dt>'
        '<dd>0x00000000 (Windows), false (Linux),'
        ' false (Android), &lt;false /&gt; (Mac)'
        '<dl><dt>Windows (Intune):</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;disabled/&gt;</dd></dl>'
        '</dd>'
        '</dl></root>')

  def testAddPolicyDetailsNoArcSupport(self):
    # Test that the entire Android-on-Chrome-OS sub-section is left out when
    # 'arc_support' is not specified.
    policy = {
        'type':
        'main',
        'name':
        'TestPolicyName',
        'caption':
        'TestPolicyCaption',
        'desc':
        'TestPolicyDesc',
        'supported_on': [{
            'product': 'chrome',
            'platform': 'linux',
            'since_version': '8',
            'until_version': '',
        }],
        'features': {
            'dynamic_refresh': False
        },
        'example_value':
        False
    }
    self.writer._AddPolicyDetails(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root><dl>'
        '<dt style="style_dt;">_test_data_type</dt>'
        '<dd>Boolean</dd>'
        '<dt style="style_dt;">_test_mac_linux_pref_name</dt>'
        '<dd style="style_.monospace;">TestPolicyName</dd>'
        '<dt style="style_dt;">_test_supported_on</dt>'
        '<dd>'
        '<ul style="style_ul;">'
        '<li>Chrome (Linux) ..8..</li>'
        '</ul>'
        '</dd>'
        '<dt style="style_dt;">_test_supported_features</dt>'
        '<dd>_test_feature_dynamic_refresh: _test_not_supported</dd>'
        '<dt style="style_dt;">_test_description</dt>'
        '<dd><p>TestPolicyDesc</p></dd>'
        '<dt style="style_dt;">_test_example_value</dt>'
        '<dd>false (Linux)</dd>'
        '</dl></root>')

  def testAddDictPolicyDetails(self):
    # Test if the definition list (<dl>) of policy details is created correctly
    # for 'dict' policies.
    policy = {
        'type':
        'dict',
        'name':
        'TestPolicyName',
        'caption':
        'TestPolicyCaption',
        'desc':
        'TestPolicyDesc',
        'schema': {
            'properties': {
                'foo': {
                    'type': 'integer'
                }
            },
            'type': 'object'
        },
        'url_schema':
        'https://example.com/details',
        'supported_on': [{
            'product': 'chrome',
            'platform': 'win',
            'since_version': '8',
            'until_version': '',
        },
                         {
                             'product': 'chrome',
                             'platform': 'mac',
                             'since_version': '8',
                             'until_version': '',
                         },
                         {
                             'product': 'chrome',
                             'platform': 'linux',
                             'since_version': '8',
                             'until_version': '',
                         },
                         {
                             'product': 'chrome_os',
                             'platform': 'chrome_os',
                             'since_version': '8',
                             'until_version': '',
                         }],
        'features': {
            'dynamic_refresh': False
        },
        'example_value': {
            'foo': 123
        }
    }
    self.writer._AddPolicyDetails(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root><dl>'
        '<dt style="style_dt;">_test_data_type</dt>'
        '<dd>Dictionary [Windows:REG_SZ] (_test_complex_policies_win)</dd>'
        '<dt style="style_dt;">_test_win_reg_loc</dt>'
        '<dd style="style_.monospace;">MockKey\TestPolicyName</dd>'
        '<dt style="style_dt;">_test_oma_uri</dt>'
        '<dd style="style_.monospace;">.\\Device\\Vendor\\MSFT\\Policy\\Config\\Chrome~Policy~chromium\\TestPolicyName</dd>'
        '<dt style="style_dt;">_test_mac_linux_pref_name</dt>'
        '<dd style="style_.monospace;">TestPolicyName</dd>'
        '<dt style="style_dt;">_test_supported_on</dt>'
        '<dd>'
        '<ul style="style_ul;">'
        '<li>Chrome (Windows) ..8..</li>'
        '<li>Chrome (Mac) ..8..</li>'
        '<li>Chrome (Linux) ..8..</li>'
        '<li>ChromeOS (ChromeOS) ..8..</li>'
        '</ul>'
        '</dd>'
        '<dt style="style_dt;">_test_supported_features</dt>'
        '<dd>_test_feature_dynamic_refresh: _test_not_supported</dd>'
        '<dt style="style_dt;">_test_description</dt><dd><p>TestPolicyDesc</p></dd>'
        '<dt style="style_dt;">_test_schema</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">{\n'
        '  &quot;properties&quot;: {\n'
        '    &quot;foo&quot;: {\n'
        '      &quot;type&quot;: &quot;integer&quot;\n'
        '    }\n'
        '  }, \n'
        '  &quot;type&quot;: &quot;object&quot;\n'
        '}</dd>'
        '<dt style="style_dt;">_test_url_schema</dt>'
        '<dd><a href="https://example.com/details">https://example.com/details</a></dd>'
        '<dt style="style_dt;">_test_example_value</dt>'
        '<dd>'
        '<dl style="style_dd dl;">'
        '<dt>_test_example_value_win</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">'
        'MockKey\TestPolicyName = {\n'
        '  &quot;foo&quot;: 123\n'
        '}'
        '</dd>'
        '<dt>Android/Linux:</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">'
        'TestPolicyName: {\n'
        '  &quot;foo&quot;: 123\n'
        '}'
        '</dd>'
        '<dt>Mac:</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">'
        '&lt;key&gt;TestPolicyName&lt;/key&gt;\n'
        '&lt;dict&gt;\n'
        '  &lt;key&gt;foo&lt;/key&gt;\n'
        '  &lt;integer&gt;123&lt;/integer&gt;\n'
        '&lt;/dict&gt;'
        '</dd>'
        '</dl>'
        '<dl><dt>Windows (Intune):</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;enabled/&gt;</dd>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;data id=&quot;TestPolicyName&quot; value=&quot;&quot;foo&quot;: 123&quot;/&gt;</dd></dl>'
        '</dd>'
        '</dl></root>')

  def testAddExternalPolicyDetails(self):
    # Test if the definition list (<dl>) of policy details is created correctly
    # for 'external' policies.
    policy = {
        'type':
        'external',
        'name':
        'TestPolicyName',
        'caption':
        'TestPolicyCaption',
        'desc':
        'TestPolicyDesc',
        'description_schema': {
            'properties': {
                'url': {
                    'type': 'string'
                },
                'hash': {
                    'type': 'string'
                },
            },
            'type': 'object'
        },
        'supported_on': [{
            'product': 'chrome',
            'platform': 'win',
            'since_version': '8',
            'until_version': '',
        },
                         {
                             'product': 'chrome',
                             'platform': 'mac',
                             'since_version': '8',
                             'until_version': '',
                         },
                         {
                             'product': 'chrome',
                             'platform': 'linux',
                             'since_version': '8',
                             'until_version': '',
                         }],
        'features': {
            'dynamic_refresh': False
        },
        'example_value': {
            "url": "https://example.com/avatar.jpg",
            "hash": "deadbeef",
        },
    }
    self.writer.messages['doc_since_version'] = {'text': '...$6...'}
    self.writer._AddPolicyDetails(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root><dl>'
        '<dt style="style_dt;">_test_data_type</dt>'
        '<dd>External data reference [Windows:REG_SZ] (_test_complex_policies_win)</dd>'
        '<dt style="style_dt;">_test_win_reg_loc</dt>'
        '<dd style="style_.monospace;">MockKey\TestPolicyName</dd>'
        '<dt style="style_dt;">_test_oma_uri</dt>'
        '<dd style="style_.monospace;">.\\Device\\Vendor\\MSFT\\Policy\\Config\\Chrome~Policy~chromium\\TestPolicyName</dd>'
        '<dt style="style_dt;">_test_mac_linux_pref_name</dt>'
        '<dd style="style_.monospace;">TestPolicyName</dd>'
        '<dt style="style_dt;">_test_supported_on</dt>'
        '<dd>'
        '<ul style="style_ul;">'
        '<li>Chrome (Windows) ...8...</li>'
        '<li>Chrome (Mac) ...8...</li>'
        '<li>Chrome (Linux) ...8...</li>'
        '</ul>'
        '</dd>'
        '<dt style="style_dt;">_test_supported_features</dt>'
        '<dd>_test_feature_dynamic_refresh: _test_not_supported</dd>'
        '<dt style="style_dt;">_test_description</dt><dd><p>TestPolicyDesc</p></dd>'
        '<dt style="style_dt;">_test_schema</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">{\n'
        '  &quot;properties&quot;: {\n'
        '    &quot;hash&quot;: {\n'
        '      &quot;type&quot;: &quot;string&quot;\n'
        '    }, \n'
        '    &quot;url&quot;: {\n'
        '      &quot;type&quot;: &quot;string&quot;\n'
        '    }\n'
        '  }, \n'
        '  &quot;type&quot;: &quot;object&quot;\n'
        '}</dd>'
        '<dt style="style_dt;">_test_example_value</dt>'
        '<dd>'
        '<dl style="style_dd dl;">'
        '<dt>_test_example_value_win</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">'
        'MockKey\TestPolicyName = {\n'
        '  &quot;hash&quot;: &quot;deadbeef&quot;, \n'
        '  &quot;url&quot;: &quot;https://example.com/avatar.jpg&quot;\n'
        '}'
        '</dd>'
        '<dt>Android/Linux:</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">'
        'TestPolicyName: {\n'
        '  &quot;hash&quot;: &quot;deadbeef&quot;, \n'
        '  &quot;url&quot;: &quot;https://example.com/avatar.jpg&quot;\n'
        '}'
        '</dd>'
        '<dt>Mac:</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">'
        '&lt;key&gt;TestPolicyName&lt;/key&gt;\n'
        '&lt;dict&gt;\n'
        '  &lt;key&gt;hash&lt;/key&gt;\n'
        '  &lt;string&gt;deadbeef&lt;/string&gt;\n'
        '  &lt;key&gt;url&lt;/key&gt;\n'
        '  &lt;string&gt;https://example.com/avatar.jpg&lt;/string&gt;\n&lt;'
        '/dict&gt;'
        '</dd>'
        '</dl>'
        '<dl><dt>Windows (Intune):</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;enabled/&gt;</dd>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;data id=&quot;TestPolicyName&quot; value=&quot;&quot;url&quot;: &quot;https://example.com/avatar.jpg&quot;, &quot;hash&quot;: &quot;deadbeef&quot;&quot;/&gt;</dd></dl>'
        '</dd>'
        '</dl></root>')

  def testAddPolicyDetailsRecommendedOnly(self):
    policy = {
        'type':
        'main',
        'name':
        'TestPolicyName',
        'caption':
        'TestPolicyCaption',
        'desc':
        'TestPolicyDesc',
        'supported_on': [{
            'product': 'chrome',
            'platform': 'win',
            'since_version': '8',
            'until_version': '',
        },
                         {
                             'product': 'chrome',
                             'platform': 'mac',
                             'since_version': '8',
                             'until_version': '',
                         },
                         {
                             'product': 'chrome',
                             'platform': 'linux',
                             'since_version': '8',
                             'until_version': '',
                         },
                         {
                             'product': 'chrome',
                             'platform': 'android',
                             'since_version': '30',
                             'until_version': '',
                         },
                         {
                             'product': 'chrome',
                             'platform': 'chrome_os',
                             'since_version': '53',
                             'until_version': '',
                         }],
        'features': {
            'dynamic_refresh': False,
            'can_be_mandatory': False,
            'can_be_recommended': True
        },
        'example_value':
        False
    }
    self.writer._AddPolicyDetails(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root><dl>'
        '<dt style="style_dt;">_test_data_type</dt>'
        '<dd>Boolean [Windows:REG_DWORD]</dd>'
        '<dt style="style_dt;">_test_win_reg_loc</dt>'
        '<dd style="style_.monospace;">MockKeyRec\TestPolicyName</dd>'
        '<dt style="style_dt;">_test_oma_uri</dt>'
        '<dd style="style_.monospace;">.\\Device\\Vendor\\MSFT\\Policy\\Config\\Chrome~Policy~chromium\\TestPolicyName</dd>'
        '<dt style="style_dt;">_test_mac_linux_pref_name</dt>'
        '<dd style="style_.monospace;">TestPolicyName</dd>'
        '<dt style="style_dt;">_test_android_restriction_name</dt>'
        '<dd style="style_.monospace;">TestPolicyName</dd>'
        '<dt style="style_dt;">_test_supported_on</dt>'
        '<dd>'
        '<ul style="style_ul;">'
        '<li>Chrome (Windows) ..8..</li>'
        '<li>Chrome (Mac) ..8..</li>'
        '<li>Chrome (Linux) ..8..</li>'
        '<li>Chrome (Android) ..30..</li>'
        '<li>Chrome (ChromeOS) ..53..</li>'
        '</ul>'
        '</dd>'
        '<dt style="style_dt;">_test_supported_features</dt>'
        '<dd>_test_feature_mandatory: _test_not_supported,'
        ' _test_feature_recommended: _test_supported,'
        ' _test_feature_dynamic_refresh: _test_not_supported</dd>'
        '<dt style="style_dt;">_test_description</dt><dd><p>TestPolicyDesc</p></dd>'
        '<dt style="style_dt;">_test_example_value</dt>'
        '<dd>0x00000000 (Windows), false (Linux),'
        ' false (Android), &lt;false /&gt; (Mac)'
        '<dl><dt>Windows (Intune):</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;disabled/&gt;</dd></dl>'
        '</dd>'
        '</dl></root>')

  def testAddPolicyRow(self):
    # Test if policies are correctly added to the summary table.
    policy = {
        'name': 'PolicyName',
        'caption': 'PolicyCaption',
        'type': 'string',
    }
    self.writer._indent_level = 3
    self.writer._AddPolicyRow(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root><tr style="style_tr;">'
        '<td style="style_td;style_td.left;padding-left: 49px;">'
        '<a href="#PolicyName">PolicyName</a>'
        '</td>'
        '<td style="style_td;style_td.right;">PolicyCaption</td>'
        '</tr></root>')
    self.setUp()
    policy = {
        'name': 'PolicyName',
        'caption': 'PolicyCaption',
        'type': 'group',
    }
    self.writer._indent_level = 2
    self.writer._AddPolicyRow(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root><tr style="style_tr;">'
        '<td colspan="2" style="style_td;style_td.left;padding-left: 35px;">'
        '<a href="#PolicyName">PolicyCaption</a>'
        '</td>'
        '</tr></root>')

  def testAddPolicySection(self):
    # Test if policy details are correctly added to the document.
    policy = {
        'name':
        'PolicyName',
        'caption':
        'PolicyCaption',
        'desc':
        'PolicyDesc',
        'type':
        'string',
        'supported_on': [{
            'product': 'chrome',
            'platform': 'win',
            'since_version': '7',
            'until_version': '',
        },
                         {
                             'product': 'chrome',
                             'platform': 'mac',
                             'since_version': '7',
                             'until_version': '',
                         },
                         {
                             'product': 'chrome_os',
                             'platform': 'chrome_os',
                             'since_version': '7',
                             'until_version': '',
                         }],
        'features': {
            'dynamic_refresh': False
        },
        'example_value':
        'False'
    }
    self.writer._AddPolicySection(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root>'
        '<div style="margin-left: 0px">'
        '<h3><a name="PolicyName"/>PolicyName</h3>'
        '<span>PolicyCaption</span>'
        '<dl>'
        '<dt style="style_dt;">_test_data_type</dt>'
        '<dd>String [Windows:REG_SZ]</dd>'
        '<dt style="style_dt;">_test_win_reg_loc</dt>'
        '<dd style="style_.monospace;">MockKey\\PolicyName</dd>'
        '<dt style="style_dt;">_test_oma_uri</dt>'
        '<dd style="style_.monospace;">.\\Device\\Vendor\\MSFT\\Policy\\Config\\Chrome~Policy~chromium\\PolicyName</dd>'
        '<dt style="style_dt;">_test_mac_linux_pref_name</dt>'
        '<dd style="style_.monospace;">PolicyName</dd>'
        '<dt style="style_dt;">_test_supported_on</dt>'
        '<dd>'
        '<ul style="style_ul;">'
        '<li>Chrome (Windows) ..7..</li>'
        '<li>Chrome (Mac) ..7..</li>'
        '<li>ChromeOS (ChromeOS) ..7..</li>'
        '</ul>'
        '</dd>'
        '<dt style="style_dt;">_test_supported_features</dt>'
        '<dd>_test_feature_dynamic_refresh: _test_not_supported</dd>'
        '<dt style="style_dt;">_test_description</dt>'
        '<dd><p>PolicyDesc</p></dd>'
        '<dt style="style_dt;">_test_example_value</dt>'
        '<dd>&quot;False&quot;'
        '<dl><dt>Windows (Intune):</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;enabled/&gt;</dd>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;data id=&quot;PolicyName&quot; value=&quot;False&quot;/&gt;</dd></dl>'
        '</dd>'
        '</dl>'
        '<a href="#top">_test_back_to_top</a>'
        '</div>'
        '</root>')
    # Test for groups.
    self.setUp()
    policy['type'] = 'group'
    self.writer._AddPolicySection(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root>'
        '<div style="margin-left: 0px">'
        '<h2><a name="PolicyName"/>PolicyCaption</h2>'
        '<div style="style_div.group_desc;">PolicyDesc</div>'
        '<a href="#top">_test_back_to_top</a>'
        '</div>'
        '</root>')

  def testAddPolicySectionWithAtomicGroup(self):
    # Test if policy details are correctly added to the document.
    policy = {
        'name':
        'PolicyName',
        'caption':
        'PolicyCaption',
        'desc':
        'PolicyDesc',
        'type':
        'string',
        'supported_on': [{
            'product': 'chrome',
            'platform': 'win',
            'since_version': '7',
            'until_version': '',
        },
                         {
                             'product': 'chrome',
                             'platform': 'mac',
                             'since_version': '7',
                             'until_version': '',
                         },
                         {
                             'product': 'chrome_os',
                             'platform': 'chrome_os',
                             'since_version': '7',
                             'until_version': '',
                         }],
        'features': {
            'dynamic_refresh': False
        },
        'example_value':
        'False',
        'atomic_group':
        'PolicyGroup'
    }
    self.writer._AddPolicySection(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root>'
        '<div style="margin-left: 0px">'
        '<h3><a name="PolicyName"/>PolicyName</h3>'
        '<span>PolicyCaption</span>'
        '<dl>'
        '<dt style="style_dt;">_test_data_type</dt>'
        '<dd>String [Windows:REG_SZ]</dd>'
        '<dt style="style_dt;">_test_win_reg_loc</dt>'
        '<dd style="style_.monospace;">MockKey\\PolicyName</dd>'
        '<dt style="style_dt;">_test_oma_uri</dt>'
        '<dd style="style_.monospace;">.\\Device\\Vendor\\MSFT\\Policy\\Config\\Chrome~Policy~chromium\\PolicyName</dd>'
        '<dt style="style_dt;">_test_mac_linux_pref_name</dt>'
        '<dd style="style_.monospace;">PolicyName</dd>'
        '<dt style="style_dt;">_test_supported_on</dt>'
        '<dd>'
        '<ul style="style_ul;">'
        '<li>Chrome (Windows) ..7..</li>'
        '<li>Chrome (Mac) ..7..</li>'
        '<li>ChromeOS (ChromeOS) ..7..</li>'
        '</ul>'
        '</dd>'
        '<dt style="style_dt;">_test_supported_features</dt>'
        '<dd>_test_feature_dynamic_refresh: _test_not_supported</dd>'
        '<dt style="style_dt;">_test_description</dt>'
        '<dd><p>PolicyDesc</p></dd>'
        '<dt style="style_dt;">_test_example_value</dt>'
        '<dd>&quot;False&quot;'
        '<dl><dt>Windows (Intune):</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;enabled/&gt;</dd>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;data id=&quot;PolicyName&quot; value=&quot;False&quot;/&gt;</dd></dl>'
        '</dd>'
        '<dt style="style_dt;">_test_policy_atomic_group</dt>'
        '<dd>_test_policy_in_atomic_group <a href="./policy-list-3/atomic_groups#PolicyGroup">PolicyGroup</a></dd>'
        '</dl>'
        '<a href="#top">_test_back_to_top</a>'
        '</div>'
        '</root>')

  def testAddPolicySectionForWindowsOnly(self):
    policy = {
        'name':
        'PolicyName',
        'caption':
        'PolicyCaption',
        'desc':
        'PolicyDesc',
        'type':
        'int',
        'supported_on': [{
            'product': 'chrome',
            'platform': 'win',
            'since_version': '33',
            'until_version': '',
        }],
        'features': {
            'dynamic_refresh': False
        },
        'example_value':
        123
    }
    self.writer._AddPolicySection(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root>'
        '<div style="margin-left: 0px">'
        '<h3><a name="PolicyName"/>PolicyName</h3>'
        '<span>PolicyCaption</span>'
        '<dl>'
        '<dt style="style_dt;">_test_data_type</dt>'
        '<dd>Integer [Windows:REG_DWORD]</dd>'
        '<dt style="style_dt;">_test_win_reg_loc</dt>'
        '<dd style="style_.monospace;">MockKey\\PolicyName</dd>'
        '<dt style="style_dt;">_test_oma_uri</dt>'
        '<dd style="style_.monospace;">.\\Device\\Vendor\\MSFT\\Policy\\Config\\Chrome~Policy~chromium\\PolicyName</dd>'
        '<dt style="style_dt;">_test_supported_on</dt>'
        '<dd>'
        '<ul style="style_ul;">'
        '<li>Chrome (Windows) ..33..</li>'
        '</ul>'
        '</dd>'
        '<dt style="style_dt;">_test_supported_features</dt>'
        '<dd>_test_feature_dynamic_refresh: _test_not_supported</dd>'
        '<dt style="style_dt;">_test_description</dt>'
        '<dd><p>PolicyDesc</p></dd>'
        '<dt style="style_dt;">_test_example_value</dt>'
        '<dd>0x0000007b (Windows)'
        '<dl><dt>Windows (Intune):</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;enabled/&gt;</dd>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;data id=&quot;PolicyName&quot; value=&quot;123&quot;/&gt;</dd></dl>'
        '</dd>'
        '</dl>'
        '<a href="#top">_test_back_to_top</a>'
        '</div>'
        '</root>')

  def testAddPolicySectionForWindows7Only(self):
    policy = {
        'name':
        'PolicyName',
        'caption':
        'PolicyCaption',
        'desc':
        'PolicyDesc',
        'type':
        'int',
        'supported_on': [{
            'product': 'chrome',
            'platform': 'win7',
            'since_version': '33',
            'until_version': '',
        }],
        'features': {
            'dynamic_refresh': False
        },
        'example_value':
        123
    }
    self.writer._AddPolicySection(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root>'
        '<div style="margin-left: 0px">'
        '<h3><a name="PolicyName"/>PolicyName</h3>'
        '<span>PolicyCaption</span>'
        '<dl>'
        '<dt style="style_dt;">_test_data_type</dt>'
        '<dd>Integer [Windows:REG_DWORD]</dd>'
        '<dt style="style_dt;">_test_win_reg_loc</dt>'
        '<dd style="style_.monospace;">MockKey\\PolicyName</dd>'
        '<dt style="style_dt;">_test_oma_uri</dt>'
        '<dd style="style_.monospace;">.\\Device\\Vendor\\MSFT\\Policy\\Config\\Chrome~Policy~chromium\\PolicyName</dd>'
        '<dt style="style_dt;">_test_supported_on</dt>'
        '<dd>'
        '<ul style="style_ul;">'
        '<li>Chrome (Windows 7) ..33..</li>'
        '</ul>'
        '</dd>'
        '<dt style="style_dt;">_test_supported_features</dt>'
        '<dd>_test_feature_dynamic_refresh: _test_not_supported</dd>'
        '<dt style="style_dt;">_test_description</dt>'
        '<dd><p>PolicyDesc</p></dd>'
        '<dt style="style_dt;">_test_example_value</dt>'
        '<dd>0x0000007b (Windows)'
        '<dl><dt>Windows (Intune):</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;enabled/&gt;</dd>'
        '<dd style="style_.monospace;style_.pre-wrap;">&lt;data id=&quot;PolicyName&quot; value=&quot;123&quot;/&gt;</dd></dl>'
        '</dd>'
        '</dl>'
        '<a href="#top">_test_back_to_top</a>'
        '</div>'
        '</root>')

  def testAddPolicySectionForMacOnly(self):
    policy = {
        'name':
        'PolicyName',
        'caption':
        'PolicyCaption',
        'desc':
        'PolicyDesc',
        'type':
        'int',
        'supported_on': [{
            'product': 'chrome',
            'platform': 'mac',
            'since_version': '33',
            'until_version': '',
        }],
        'features': {
            'dynamic_refresh': False
        },
        'example_value':
        123
    }
    self.writer._AddPolicySection(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root>'
        '<div style="margin-left: 0px">'
        '<h3><a name="PolicyName"/>PolicyName</h3>'
        '<span>PolicyCaption</span>'
        '<dl>'
        '<dt style="style_dt;">_test_data_type</dt>'
        '<dd>Integer</dd>'
        '<dt style="style_dt;">_test_mac_linux_pref_name</dt>'
        '<dd style="style_.monospace;">PolicyName</dd>'
        '<dt style="style_dt;">_test_supported_on</dt>'
        '<dd>'
        '<ul style="style_ul;">'
        '<li>Chrome (Mac) ..33..</li>'
        '</ul>'
        '</dd>'
        '<dt style="style_dt;">_test_supported_features</dt>'
        '<dd>_test_feature_dynamic_refresh: _test_not_supported</dd>'
        '<dt style="style_dt;">_test_description</dt>'
        '<dd><p>PolicyDesc</p></dd>'
        '<dt style="style_dt;">_test_example_value</dt>'
        '<dd>123 (Mac)</dd>'
        '</dl>'
        '<a href="#top">_test_back_to_top</a>'
        '</div>'
        '</root>')

  def testAddPolicySectionForLinuxOnly(self):
    policy = {
        'name':
        'PolicyName',
        'caption':
        'PolicyCaption',
        'desc':
        'PolicyDesc',
        'type':
        'int',
        'supported_on': [{
            'product': 'chrome',
            'platform': 'linux',
            'since_version': '33',
            'until_version': '',
        }],
        'features': {
            'dynamic_refresh': False
        },
        'example_value':
        123
    }
    self.writer._AddPolicySection(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root>'
        '<div style="margin-left: 0px">'
        '<h3><a name="PolicyName"/>PolicyName</h3>'
        '<span>PolicyCaption</span>'
        '<dl>'
        '<dt style="style_dt;">_test_data_type</dt>'
        '<dd>Integer</dd>'
        '<dt style="style_dt;">_test_mac_linux_pref_name</dt>'
        '<dd style="style_.monospace;">PolicyName</dd>'
        '<dt style="style_dt;">_test_supported_on</dt>'
        '<dd>'
        '<ul style="style_ul;">'
        '<li>Chrome (Linux) ..33..</li>'
        '</ul>'
        '</dd>'
        '<dt style="style_dt;">_test_supported_features</dt>'
        '<dd>_test_feature_dynamic_refresh: _test_not_supported</dd>'
        '<dt style="style_dt;">_test_description</dt>'
        '<dd><p>PolicyDesc</p></dd>'
        '<dt style="style_dt;">_test_example_value</dt>'
        '<dd>123 (Linux)</dd>'
        '</dl>'
        '<a href="#top">_test_back_to_top</a>'
        '</div>'
        '</root>')

  def testAddPolicySectionForAndroidOnly(self):
    policy = {
        'name':
        'PolicyName',
        'caption':
        'PolicyCaption',
        'desc':
        'PolicyDesc',
        'type':
        'int',
        'supported_on': [{
            'product': 'chrome',
            'platform': 'android',
            'since_version': '33',
            'until_version': '',
        }],
        'features': {
            'dynamic_refresh': False
        },
        'example_value':
        123
    }
    self.writer._AddPolicySection(self.doc_root, policy)
    self.assertTrue(
        self.writer.IsPolicyOrItemSupportedOnPlatform(policy, 'android'))
    self.assertEquals(
        self.doc_root.toxml(), '<root>'
        '<div style="margin-left: 0px">'
        '<h3><a name="PolicyName"/>PolicyName</h3>'
        '<span>PolicyCaption</span>'
        '<dl>'
        '<dt style="style_dt;">_test_data_type</dt>'
        '<dd>Integer</dd>'
        '<dt style="style_dt;">_test_android_restriction_name</dt>'
        '<dd style="style_.monospace;">PolicyName</dd>'
        '<dt style="style_dt;">_test_supported_on</dt>'
        '<dd>'
        '<ul style="style_ul;">'
        '<li>Chrome (Android) ..33..</li>'
        '</ul>'
        '</dd>'
        '<dt style="style_dt;">_test_supported_features</dt>'
        '<dd>_test_feature_dynamic_refresh: _test_not_supported</dd>'
        '<dt style="style_dt;">_test_description</dt>'
        '<dd><p>PolicyDesc</p></dd>'
        '<dt style="style_dt;">_test_example_value</dt>'
        '<dd>123 (Android)</dd>'
        '</dl>'
        '<a href="#top">_test_back_to_top</a>'
        '</div>'
        '</root>')

  def testAddDictionaryExample(self):
    policy = {
        'name':
        'PolicyName',
        'caption':
        'PolicyCaption',
        'desc':
        'PolicyDesc',
        'type':
        'dict',
        'supported_on': [{
            'product': 'chrome',
            'platform': 'win',
            'since_version': '7',
            'until_version': '',
        },
                         {
                             'product': 'chrome',
                             'platform': 'mac',
                             'since_version': '7',
                             'until_version': '',
                         },
                         {
                             'product': 'chrome',
                             'platform': 'linux',
                             'since_version': '7',
                             'until_version': '',
                         }],
        'features': {
            'dynamic_refresh': False
        },
        'example_value': {
            "ProxyMode": "direct",
            "List": ["1", "2", "3"],
            "True": True,
            "False": False,
            "Integer": 123,
            "DictList": [
                {
                    "A": 1,
                    "B": 2,
                },
                {
                    "C": 3,
                    "D": 4,
                },
            ],
        },
    }
    self.writer._AddDictionaryExample(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root>'
        '<dl style="style_dd dl;">'
        '<dt>_test_example_value_win</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">MockKey\PolicyName = {\n'
        '  &quot;DictList&quot;: [\n'
        '    {\n'
        '      &quot;A&quot;: 1, \n'
        '      &quot;B&quot;: 2\n'
        '    }, \n'
        '    {\n'
        '      &quot;C&quot;: 3, \n'
        '      &quot;D&quot;: 4\n'
        '    }\n'
        '  ], \n'
        '  &quot;False&quot;: false, \n'
        '  &quot;Integer&quot;: 123, \n'
        '  &quot;List&quot;: [\n'
        '    &quot;1&quot;, \n'
        '    &quot;2&quot;, \n'
        '    &quot;3&quot;\n'
        '  ], \n'
        '  &quot;ProxyMode&quot;: &quot;direct&quot;, \n'
        '  &quot;True&quot;: true\n'
        '}'
        '</dd>'
        '<dt>Android/Linux:</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">PolicyName: {\n'
        '  &quot;DictList&quot;: [\n'
        '    {\n'
        '      &quot;A&quot;: 1, \n'
        '      &quot;B&quot;: 2\n'
        '    }, \n'
        '    {\n'
        '      &quot;C&quot;: 3, \n'
        '      &quot;D&quot;: 4\n'
        '    }\n'
        '  ], \n'
        '  &quot;False&quot;: false, \n'
        '  &quot;Integer&quot;: 123, \n'
        '  &quot;List&quot;: [\n'
        '    &quot;1&quot;, \n'
        '    &quot;2&quot;, \n'
        '    &quot;3&quot;\n'
        '  ], \n'
        '  &quot;ProxyMode&quot;: &quot;direct&quot;, \n'
        '  &quot;True&quot;: true\n'
        '}'
        '</dd>'
        '<dt>Mac:</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">'
        '&lt;key&gt;PolicyName&lt;/key&gt;\n'
        '&lt;dict&gt;\n'
        '  &lt;key&gt;DictList&lt;/key&gt;\n'
        '  &lt;array&gt;\n'
        '    &lt;dict&gt;\n'
        '      &lt;key&gt;A&lt;/key&gt;\n'
        '      &lt;integer&gt;1&lt;/integer&gt;\n'
        '      &lt;key&gt;B&lt;/key&gt;\n'
        '      &lt;integer&gt;2&lt;/integer&gt;\n'
        '    &lt;/dict&gt;\n'
        '    &lt;dict&gt;\n'
        '      &lt;key&gt;C&lt;/key&gt;\n'
        '      &lt;integer&gt;3&lt;/integer&gt;\n'
        '      &lt;key&gt;D&lt;/key&gt;\n'
        '      &lt;integer&gt;4&lt;/integer&gt;\n'
        '    &lt;/dict&gt;\n'
        '  &lt;/array&gt;\n'
        '  &lt;key&gt;False&lt;/key&gt;\n'
        '  &lt;false/&gt;\n'
        '  &lt;key&gt;Integer&lt;/key&gt;\n'
        '  &lt;integer&gt;123&lt;/integer&gt;\n'
        '  &lt;key&gt;List&lt;/key&gt;\n'
        '  &lt;array&gt;\n'
        '    &lt;string&gt;1&lt;/string&gt;\n'
        '    &lt;string&gt;2&lt;/string&gt;\n'
        '    &lt;string&gt;3&lt;/string&gt;\n'
        '  &lt;/array&gt;\n'
        '  &lt;key&gt;ProxyMode&lt;/key&gt;\n'
        '  &lt;string&gt;direct&lt;/string&gt;\n'
        '  &lt;key&gt;True&lt;/key&gt;\n'
        '  &lt;true/&gt;\n'
        '&lt;/dict&gt;'
        '</dd>'
        '</dl>'
        '</root>')

  def testAddExternalExample(self):
    policy = {
        'name':
        'PolicyName',
        'caption':
        'PolicyCaption',
        'desc':
        'PolicyDesc',
        'type':
        'external',
        'supported_on': [{
            'product': 'chrome',
            'platform': 'win',
            'since_version': '7',
            'until_version': '',
        },
                         {
                             'product': 'chrome',
                             'platform': 'mac',
                             'since_version': '7',
                             'until_version': '',
                         },
                         {
                             'product': 'chrome',
                             'platform': 'linux',
                             'since_version': '7',
                             'until_version': '',
                         }],
        'features': {
            'dynamic_refresh': False
        },
        'example_value': {
            "url": "https://example.com/avatar.jpg",
            "hash": "deadbeef",
        },
    }
    self.writer._AddDictionaryExample(self.doc_root, policy)
    self.assertEquals(
        self.doc_root.toxml(), '<root>'
        '<dl style="style_dd dl;">'
        '<dt>_test_example_value_win</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">MockKey\PolicyName = {\n'
        '  &quot;hash&quot;: &quot;deadbeef&quot;, \n'
        '  &quot;url&quot;: &quot;https://example.com/avatar.jpg&quot;\n'
        '}'
        '</dd>'
        '<dt>Android/Linux:</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">PolicyName: {\n'
        '  &quot;hash&quot;: &quot;deadbeef&quot;, \n'
        '  &quot;url&quot;: &quot;https://example.com/avatar.jpg&quot;\n'
        '}'
        '</dd>'
        '<dt>Mac:</dt>'
        '<dd style="style_.monospace;style_.pre-wrap;">'
        '&lt;key&gt;PolicyName&lt;/key&gt;\n'
        '&lt;dict&gt;\n'
        '  &lt;key&gt;hash&lt;/key&gt;\n'
        '  &lt;string&gt;deadbeef&lt;/string&gt;\n'
        '  &lt;key&gt;url&lt;/key&gt;\n'
        '  &lt;string&gt;https://example.com/avatar.jpg&lt;/string&gt;\n'
        '&lt;/dict&gt;'
        '</dd>'
        '</dl>'
        '</root>')

  def testParagraphs(self):
    text = 'Paragraph 1\n\nParagraph 2\n\nParagraph 3'
    self.writer._AddParagraphs(self.doc_root, text)
    self.assertEquals(
        self.doc_root.toxml(),
        '<root><p>Paragraph 1</p><p>Paragraph 2</p><p>Paragraph 3</p></root>')


if __name__ == '__main__':
  unittest.main()
