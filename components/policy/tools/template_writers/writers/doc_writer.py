#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import re
from xml.dom import minidom
from xml.sax.saxutils import escape
from writers import xml_formatted_writer


def GetWriter(config):
  '''Factory method for creating DocWriter objects.
  See the constructor of TemplateWriter for description of
  arguments.
  '''
  return DocWriter(['*'], config)


class DocWriter(xml_formatted_writer.XMLFormattedWriter):
  '''Class for generating policy templates in HTML format.
  The intended use of the generated file is to upload it on
  http://dev.chromium.org, therefore its format has some limitations:
  - No HTML and body tags.
  - Restricted set of element attributes: for example no 'class'.
  Because of the latter the output is styled using the 'style'
  attributes of HTML elements. This is supported by the dictionary
  self._STYLES[] and the method self._AddStyledElement(), they try
  to mimic the functionality of CSS classes. (But without inheritance.)

  This class is invoked by PolicyTemplateGenerator to create the HTML
  files.
  '''

  def _MapListToString(self, item_map, items):
    '''Creates a comma-separated list.

    Args:
      item_map: A dictionary containing all the elements of 'items' as
        keys.
      items: A list of arbitrary items.

    Returns:
      Looks up each item of 'items' in 'item_maps' and concatenates the
      resulting items into a comma-separated list.
    '''
    return ', '.join([item_map[x] for x in items])

  def _AddTextWithLinks(self, parent, text):
    '''Parse a string for URLs and add it to a DOM node with the URLs replaced
    with <a> HTML links.

    Args:
      parent: The DOM node to which the text will be added.
      text: The string to be added.
    '''
    # A simple regexp to search for URLs. It is enough for now.
    url_matcher = re.compile('(https?://[^\\s]*[^\\s\\.\\)\\"])')

    # Iterate through all the URLs and replace them with links.
    while True:
      # Look for the first URL.
      res = url_matcher.search(text)
      if not res:
        break
      # Calculate positions of the substring of the URL.
      url = res.group(0)
      start = res.start(0)
      end = res.end(0)
      # Add the text prior to the URL.
      self.AddText(parent, text[:start])
      # Add a link for the URL.
      self.AddElement(parent, 'a', {'href': url}, url)
      # Drop the part of text that is added.
      text = text[end:]
    self.AddText(parent, text)

  def _AddParagraphs(self, parent, text):
    '''Break description into paragraphs and replace URLs with links.

    Args:
      parent: The DOM node to which the text will be added.
      text: The string to be added.
    '''
    # Split text into list of paragraphs.
    entries = text.split('\n\n')
    for entry in entries:
      # Create a new paragraph node.
      paragraph = self.AddElement(parent, 'p')
      # Insert text to the paragraph with processing the URLs.
      self._AddTextWithLinks(paragraph, entry)

  def _AddStyledElement(self, parent, name, style_ids, attrs=None, text=None):
    '''Adds an XML element to a parent, with CSS style-sheets included.

    Args:
      parent: The parent DOM node.
      name: Name of the element to add.
      style_ids: A list of CSS style strings from self._STYLE[].
      attrs: Dictionary of attributes for the element.
      text: Text content for the element.
    '''
    if attrs == None:
      attrs = {}

    style = ''.join([self._STYLE[x] for x in style_ids])
    if style != '':
      # Apply the style specified by style_ids.
      attrs['style'] = style + attrs.get('style', '')
    return self.AddElement(parent, name, attrs, text)

  def _AddDescription(self, parent, policy):
    '''Adds a string containing the description of the policy. URLs are
    replaced with links and the possible choices are enumerated in case
    of 'string-enum' and 'int-enum' type policies.

    Args:
      parent: The DOM node for which the feature list will be added.
      policy: The data structure of a policy.
    '''
    # Add description by paragraphs (URLs will be substituted by links).
    self._AddParagraphs(parent, policy['desc'])
    # Add list of enum items.
    if policy['type'] in ('string-enum', 'int-enum', 'string-enum-list'):
      ul = self.AddElement(parent, 'ul')
      for item in policy['items']:
        if policy['type'] == 'int-enum':
          value_string = str(item['value'])
        else:
          value_string = '"%s"' % item['value']
        self.AddElement(ul, 'li', {},
                        '%s = %s' % (value_string, item['caption']))

  def _AddSchema(self, parent, schema):
    '''Adds a schema to a DOM node.

    Args:
      parent: The DOM node for which the schema will be added.
      schema: The schema of a policy.
    '''
    dd = self._AddPolicyAttribute(parent, 'schema', None,
                                  ['.monospace', '.pre-wrap'])
    schema_json = json.dumps(schema, indent=2, sort_keys=True)
    self.AddText(dd, schema_json)

  def _AddFeatures(self, parent, policy):
    '''Adds a string containing the list of supported features of a policy
    to a DOM node. The text will look like as:
      Feature_X: Yes, Feature_Y: No

    Args:
      parent: The DOM node for which the feature list will be added.
      policy: The data structure of a policy.
    '''
    features = []
    # The sorting is to make the order well-defined for testing.
    keys = policy['features'].keys()
    keys.sort()
    for key in keys:
      key_name = self._FEATURE_MAP[key]
      if policy['features'][key]:
        value_name = self.GetLocalizedMessage('supported')
      else:
        value_name = self.GetLocalizedMessage('not_supported')
      features.append('%s: %s' % (key_name, value_name))
    self.AddText(parent, ', '.join(features))

  def _AddListExampleMac(self, parent, policy):
    '''Adds an example value for Mac of a 'list' policy to a DOM node.

    Args:
      parent: The DOM node for which the example will be added.
      policy: A policy of type 'list', for which the Mac example value
        is generated.
    '''
    example_value = policy['example_value']
    self.AddElement(parent, 'dt', {}, 'Mac:')
    mac = self._AddStyledElement(parent, 'dd', ['.monospace', '.pre-wrap'])

    mac_text = ['<array>']
    for item in example_value:
      mac_text.append('  <string>%s</string>' % item)
    mac_text.append('</array>')
    self.AddText(mac, '\n'.join(mac_text))

  def _AddListExampleWindowsChromeOS(self, parent, policy, is_win):
    '''Adds an example value for Windows or Chromium/Google Chrome OS of a
    'list' policy to a DOM node.

    Args:
      parent: The DOM node for which the example will be added.
      policy: A policy of type 'list', for which the Windows example value
        is generated.
      is_win: True for Windows, False for Chromium/Google Chrome OS.
    '''
    example_value = policy['example_value']
    os_header = self.GetLocalizedMessage('win_example_value') if is_win else \
                self.GetLocalizedMessage('chrome_os_example_value')
    self.AddElement(parent, 'dt', {}, os_header)
    element = self._AddStyledElement(parent, 'dd', ['.monospace', '.pre-wrap'])
    element_text = []
    cnt = 1
    key_name = self._GetRegistryKeyName(policy, is_win)
    for item in example_value:
      element_text.append(
          '%s\\%s\\%d = "%s"' % (key_name, policy['name'], cnt, item))
      cnt = cnt + 1
    self.AddText(element, '\n'.join(element_text))

  def _GetRegistryKeyName(self, policy, is_win):
    use_recommended_key = self.CanBeRecommended(policy) and not \
                          self.CanBeMandatory(policy)
    platform = 'win' if is_win else 'chrome_os'
    key = 'reg_recommended_key_name' if use_recommended_key else \
          'reg_mandatory_key_name'
    return self.config['win_config'][platform][key]

  def _GetOmaUriPath(self, policy):
    product = 'googlechrome' if self.config['build'] == 'chrome' else 'chromium'
    group = '~' + policy['group'] if 'group' in policy else ''
    return '.\\Device\\Vendor\\MSFT\\Policy\\Config\\Chrome~Policy~%s%s\\%s' % (
        product, group, policy['name'])

  def _AddListExampleAndroidLinux(self, parent, policy):
    '''Adds an example value for Android/Linux of a 'list' policy to a DOM node.

    Args:
      parent: The DOM node for which the example will be added.
      policy: A policy of type 'list', for which the Android/Linux example value
        is generated.
    '''
    example_value = policy['example_value']
    self.AddElement(parent, 'dt', {}, 'Android/Linux:')
    element = self._AddStyledElement(parent, 'dd', ['.monospace', '.pre-wrap'])
    self.AddText(
        element,
        '[\n%s\n]' % ',\n'.join('  "%s"' % item for item in example_value))

  def _AddListExample(self, parent, policy):
    '''Adds the example value of a 'list' policy to a DOM node. Example output:
    <dl>
      <dt>Windows (Windows clients):</dt>
      <dd>
        Software\Policies\Chromium\DisabledPlugins\0 = "Java"
        Software\Policies\Chromium\DisabledPlugins\1 = "Shockwave Flash"
      </dd>
      <dt>Windows (Chromium OS clients):</dt>
      <dd>
        Software\Policies\ChromiumOS\DisabledPlugins\0 = "Java"
        Software\Policies\ChromiumOS\DisabledPlugins\1 = "Shockwave Flash"
      </dd>
      <dt>Android/Linux:</dt>
      <dd>
        [
          "Java",
          "Shockwave Flash"
        ]
      </dd>
      <dt>Mac:</dt>
      <dd>
        <array>
          <string>Java</string>
          <string>Shockwave Flash</string>
        </array>
      </dd>
    </dl>

    Args:
      parent: The DOM node for which the example will be added.
      policy: The data structure of a policy.
    '''
    examples = self._AddStyledElement(parent, 'dl', ['dd dl'])
    if self.IsPolicySupportedOnWindows(policy):
      self._AddListExampleWindowsChromeOS(examples, policy, True)
    if self.IsPolicyOrItemSupportedOnPlatform(
        policy, 'chrome_os', management='active_directory'):
      self._AddListExampleWindowsChromeOS(examples, policy, False)
    if (self.IsPolicyOrItemSupportedOnPlatform(policy, 'android') or
        self.IsPolicyOrItemSupportedOnPlatform(policy, 'linux')):
      self._AddListExampleAndroidLinux(examples, policy)
    if self.IsPolicyOrItemSupportedOnPlatform(policy, 'mac'):
      self._AddListExampleMac(examples, policy)

  def _PythonObjectToPlist(self, obj, indent=''):
    '''Converts a python object to an equivalent XML plist.

    Returns a list of lines.'''
    obj_type = type(obj)
    if obj_type == bool:
      return ['%s<%s/>' % (indent, 'true' if obj else 'false')]
    elif obj_type == int:
      return ['%s<integer>%s</integer>' % (indent, obj)]
    elif obj_type == str:
      return ['%s<string>%s</string>' % (indent, escape(obj))]
    elif obj_type == list:
      result = ['%s<array>' % indent]
      for item in obj:
        result += self._PythonObjectToPlist(item, indent + '  ')
      result.append('%s</array>' % indent)
      return result
    elif obj_type == dict:
      result = ['%s<dict>' % indent]
      for key in sorted(obj.keys()):
        result.append('%s<key>%s</key>' % (indent + '  ', key))
        result += self._PythonObjectToPlist(obj[key], indent + '  ')
      result.append('%s</dict>' % indent)
      return result
    else:
      raise Exception('Invalid object to convert: %s' % obj)

  def _AddDictionaryExampleMac(self, parent, policy):
    '''Adds an example value for Mac of a 'dict' or 'external' policy to a DOM
    node.

    Args:
      parent: The DOM node for which the example will be added.
      policy: A policy of type 'dict', for which the Mac example value
        is generated.
    '''
    example_value = policy['example_value']
    self.AddElement(parent, 'dt', {}, 'Mac:')
    mac = self._AddStyledElement(parent, 'dd', ['.monospace', '.pre-wrap'])
    mac_text = ['<key>%s</key>' % (policy['name'])]
    mac_text += self._PythonObjectToPlist(example_value)
    self.AddText(mac, '\n'.join(mac_text))

  def _AddDictionaryExampleWindowsChromeOS(self, parent, policy, is_win):
    '''Adds an example value for Windows of a 'dict' or 'external' policy to a
    DOM node.

    Args:
      parent: The DOM node for which the example will be added.
      policy: A policy of type 'dict', for which the Windows example value
        is generated.
    '''
    os_header = self.GetLocalizedMessage('win_example_value') if is_win else \
                self.GetLocalizedMessage('chrome_os_example_value')
    self.AddElement(parent, 'dt', {}, os_header)
    element = self._AddStyledElement(parent, 'dd', ['.monospace', '.pre-wrap'])
    key_name = self._GetRegistryKeyName(policy, is_win)
    example = json.dumps(policy['example_value'], indent=2, sort_keys=True)
    self.AddText(element, '%s\\%s = %s' % (key_name, policy['name'], example))

  def _AddDictionaryExampleAndroidLinux(self, parent, policy):
    '''Adds an example value for Android/Linux of a 'dict' or 'external' policy
    to a DOM node.

    Args:
      parent: The DOM node for which the example will be added.
      policy: A policy of type 'dict', for which the Android/Linux example value
        is generated.
    '''
    self.AddElement(parent, 'dt', {}, 'Android/Linux:')
    element = self._AddStyledElement(parent, 'dd', ['.monospace', '.pre-wrap'])
    example = json.dumps(policy['example_value'], indent=2, sort_keys=True)
    self.AddText(element, '%s: %s' % (policy['name'], example))

  def _AddDictionaryExample(self, parent, policy):
    '''Adds the example value of a 'dict' or 'external' policy to a DOM node.

    Example output:
    <dl>
      <dt>Windows (Windows clients):</dt>
      <dd>
        Software\Policies\Chromium\ProxySettings = {
          "ProxyMode": "direct"
        }
      </dd>
      <dt>Windows (Chromium OS clients):</dt>
      <dd>
        Software\Policies\ChromiumOS\ProxySettings = {
          "ProxyMode": "direct"
        }
      </dd>
      <dt>Android/Linux:</dt>
      <dd>
        ProxySettings: {
          "ProxyMode": "direct"
        }
      </dd>
      <dt>Mac:</dt>
      <dd>
        <key>ProxySettings</key>
        <dict>
          <key>ProxyMode</key>
          <string>direct</string>
        </dict>
      </dd>
    </dl>

    Args:
      parent: The DOM node for which the example will be added.
      policy: The data structure of a policy.
    '''
    examples = self._AddStyledElement(parent, 'dl', ['dd dl'])
    if self.IsPolicySupportedOnWindows(policy):
      self._AddDictionaryExampleWindowsChromeOS(examples, policy, True)
    if self.IsPolicyOrItemSupportedOnPlatform(
        policy, 'chrome_os', management='active_directory'):
      self._AddDictionaryExampleWindowsChromeOS(examples, policy, False)
    if (self.IsPolicyOrItemSupportedOnPlatform(policy, 'android') or
        self.IsPolicyOrItemSupportedOnPlatform(policy, 'linux')):
      self._AddDictionaryExampleAndroidLinux(examples, policy)
    if self.IsPolicyOrItemSupportedOnPlatform(policy, 'mac'):
      self._AddDictionaryExampleMac(examples, policy)

  def _AddIntuneExample(self, parent, policy):
    example_value = policy['example_value']
    policy_type = policy['type']

    container = self._AddStyledElement(parent, 'dl', [])
    self.AddElement(container, 'dt', {}, 'Windows (Intune):')

    if policy_type == 'main':
      self._AddStyledElement(
          container,
          'dd', ['.monospace', '.pre-wrap'],
          text='<enabled/>' if example_value else '<disabled/>')
      return

    self._AddStyledElement(
        container, 'dd', ['.monospace', '.pre-wrap'], text='<enabled/>')
    if policy_type == 'list':
      values = [
          '%s&#xF000;%s' % (index, value)
          for index, value in enumerate(example_value, start=1)
      ]
      self._AddStyledElement(
          container,
          'dd', ['.monospace', '.pre-wrap'],
          text='<data id="%s" value="%s"/>' % (policy['name'] + 'Desc',
                                               '&#xF000;'.join(values)))
      return
    elif policy_type == 'int' or policy_type == 'int-enum':
      self._AddStyledElement(
          container,
          'dd', ['.monospace', '.pre-wrap'],
          text='<data id="%s" value="%s"/>' % (policy['name'], example_value))
    else:
      self._AddStyledElement(
          container,
          'dd', ['.monospace', '.pre-wrap'],
          text='<data id="%s" value="%s"/>' % (policy['name'],
                                               json.dumps(example_value)[1:-1]))
      return

  def _AddExample(self, parent, policy):
    '''Adds the HTML DOM representation of the example value of a policy to
    a DOM node. It is simple text for boolean policies, like
    '0x00000001 (Windows), true (Linux), true (Android), <true /> (Mac)'
    in case of boolean policies, but it may also contain other HTML elements.
    (See method _AddListExample.)

    Args:
      parent: The DOM node for which the example will be added.
      policy: The data structure of a policy.

    Raises:
      Exception: If the type of the policy is unknown or the example value
        of the policy is out of its expected range.
    '''
    example_value = policy['example_value']
    policy_type = policy['type']
    if policy_type == 'main':
      pieces = []
      if self.IsPolicySupportedOnWindows(policy) or \
         self.IsPolicyOrItemSupportedOnPlatform(policy, 'chrome_os',
                                          management='active_directory'):
        value = '0x00000001' if example_value else '0x00000000'
        pieces.append(value + ' (Windows)')
      if self.IsPolicyOrItemSupportedOnPlatform(policy, 'linux'):
        value = 'true' if example_value else 'false'
        pieces.append(value + ' (Linux)')
      if self.IsPolicyOrItemSupportedOnPlatform(policy, 'android'):
        value = 'true' if example_value else 'false'
        pieces.append(value + ' (Android)')
      if self.IsPolicyOrItemSupportedOnPlatform(policy, 'mac'):
        value = '<true />' if example_value else '<false />'
        pieces.append(value + ' (Mac)')
      self.AddText(parent, ', '.join(pieces))
    elif policy_type == 'string':
      self.AddText(parent, '"%s"' % example_value)
    elif policy_type in ('int', 'int-enum'):
      pieces = []
      if self.IsPolicySupportedOnWindows(policy) or \
         self.IsPolicyOrItemSupportedOnPlatform(policy, 'chrome_os',
                                          management='active_directory'):
        pieces.append('0x%08x (Windows)' % example_value)
      if self.IsPolicyOrItemSupportedOnPlatform(policy, 'linux'):
        pieces.append('%d (Linux)' % example_value)
      if self.IsPolicyOrItemSupportedOnPlatform(policy, 'android'):
        pieces.append('%d (Android)' % example_value)
      if self.IsPolicyOrItemSupportedOnPlatform(policy, 'mac'):
        pieces.append('%d (Mac)' % example_value)
      self.AddText(parent, ', '.join(pieces))
    elif policy_type == 'string-enum':
      self.AddText(parent, '"%s"' % (example_value))
    elif policy_type in ('list', 'string-enum-list'):
      self._AddListExample(parent, policy)
    elif policy_type in ('dict', 'external'):
      self._AddDictionaryExample(parent, policy)
    else:
      raise Exception('Unknown policy type: ' + policy_type)

    if self.IsPolicySupportedOnWindows(policy):
      self._AddIntuneExample(parent, policy)

  def _AddPolicyAttribute(self,
                          dl,
                          term_id,
                          definition=None,
                          definition_style=None):
    '''Adds a term-definition pair to a HTML DOM <dl> node. This method is
    used by _AddPolicyDetails. Its result will have the form of:
      <dt style="...">...</dt>
      <dd style="...">...</dd>

    Args:
      dl: The DOM node of the <dl> list.
      term_id: A key to self._STRINGS[] which specifies the term of the pair.
      definition: The text of the definition. (Optional.)
      definition_style: List of references to values self._STYLE[] that specify
        the CSS stylesheet of the <dd> (definition) element.

    Returns:
      The DOM node representing the definition <dd> element.
    '''
    # Avoid modifying the default value of definition_style.
    if definition_style == None:
      definition_style = []
    term = self.GetLocalizedMessage(term_id)
    self._AddStyledElement(dl, 'dt', ['dt'], {}, term)
    return self._AddStyledElement(dl, 'dd', definition_style, {}, definition)

  def _AddSupportedOnList(self, parent, supported_on_list):
    '''Creates a HTML list containing the platforms, products and versions
    that are specified in the list of supported_on.

    Args:
      parent: The DOM node for which the list will be added.
      supported_on_list: The list of supported products, as a list of
        dictionaries.
    '''
    ul = self._AddStyledElement(parent, 'ul', ['ul'])
    for supported_on in supported_on_list:
      text = []
      product = supported_on['product']
      platforms = supported_on['platforms']
      text.append(self._PRODUCT_MAP[product])
      text.append('(%s)' % self._MapListToString(self._PLATFORM_MAP, platforms))
      if supported_on['since_version']:
        since_version = self.GetLocalizedMessage('since_version')
        text.append(since_version.replace('$6', supported_on['since_version']))
      if supported_on['until_version']:
        until_version = self.GetLocalizedMessage('until_version')
        text.append(until_version.replace('$6', supported_on['until_version']))
      # Add the list element:
      self.AddElement(ul, 'li', {}, ' '.join(text))

  def _AddRangeRestrictionsList(self, parent, schema):
    '''Creates a HTML list containing range restrictions for an integer type
    policy.

    Args:
      parent: The DOM node for which the list will be added.
      schema: The schema of the policy.
    '''
    ul = self._AddStyledElement(parent, 'ul', ['ul'])
    if 'minimum' in schema:
      text_min = self.GetLocalizedMessage('range_minimum')
      self.AddElement(ul, 'li', {}, text_min + str(schema['minimum']))
    if 'maximum' in schema:
      text_max = self.GetLocalizedMessage('range_maximum')
      self.AddElement(ul, 'li', {}, text_max + str(schema['maximum']))

  def _AddPolicyDetails(self, parent, policy):
    '''Adds the list of attributes of a policy to the HTML DOM node parent.
    It will have the form:
    <dl>
      <dt>Attribute:</dt><dd>Description</dd>
      ...
    </dl>

    Args:
      parent: A DOM element for which the list will be added.
      policy: The data structure of the policy.
    '''

    dl = self.AddElement(parent, 'dl')
    data_type = [self._TYPE_MAP[policy['type']]]
    qualified_types = []
    is_complex_policy = False
    if (self.IsPolicyOrItemSupportedOnPlatform(policy, 'android') and
        self._RESTRICTION_TYPE_MAP.get(policy['type'], None)):
      qualified_types.append(
          'Android:%s' % self._RESTRICTION_TYPE_MAP[policy['type']])
      if policy['type'] in ('dict', 'external', 'list'):
        is_complex_policy = True
    if ((self.IsPolicySupportedOnWindows(policy) or
         self.IsPolicyOrItemSupportedOnPlatform(
             policy, 'chrome_os', management='active_directory')) and
        self._REG_TYPE_MAP.get(policy['type'], None)):
      qualified_types.append('Windows:%s' % self._REG_TYPE_MAP[policy['type']])
      if policy['type'] in ('dict', 'external'):
        is_complex_policy = True
    if qualified_types:
      data_type.append('[%s]' % ', '.join(qualified_types))
      if is_complex_policy:
        data_type.append(
            '(%s)' % self.GetLocalizedMessage('complex_policies_on_windows'))
    self._AddPolicyAttribute(dl, 'data_type', ' '.join(data_type))
    if self.IsPolicySupportedOnWindows(policy):
      registry_key_name = self._GetRegistryKeyName(policy, True)
      self._AddPolicyAttribute(dl, 'win_reg_loc',
                               registry_key_name + '\\' + policy['name'],
                               ['.monospace'])
      self._AddPolicyAttribute(dl, 'oma_uri', self._GetOmaUriPath(policy),
                               ['.monospace'])

    if self.IsPolicyOrItemSupportedOnPlatform(
        policy, 'chrome_os', management='active_directory'):
      key_name = self._GetRegistryKeyName(policy, False)
      self._AddPolicyAttribute(dl, 'chrome_os_reg_loc',
                               key_name + '\\' + policy['name'], ['.monospace'])
    if (self.IsPolicyOrItemSupportedOnPlatform(policy, 'linux') or
        self.IsPolicyOrItemSupportedOnPlatform(policy, 'mac')):
      self._AddPolicyAttribute(dl, 'mac_linux_pref_name', policy['name'],
                               ['.monospace'])
    if self.IsPolicyOrItemSupportedOnPlatform(
        policy, 'android', product='chrome'):
      self._AddPolicyAttribute(dl, 'android_restriction_name', policy['name'],
                               ['.monospace'])
    if self.IsPolicyOrItemSupportedOnPlatform(
        policy, 'android', product='webview'):
      restriction_prefix = self.config['android_webview_restriction_prefix']
      self._AddPolicyAttribute(dl, 'android_webview_restriction_name',
                               restriction_prefix + policy['name'],
                               ['.monospace'])
    dd = self._AddPolicyAttribute(dl, 'supported_on')
    self._AddSupportedOnList(dd, policy['supported_on'])
    dd = self._AddPolicyAttribute(dl, 'supported_features')
    self._AddFeatures(dd, policy)
    dd = self._AddPolicyAttribute(dl, 'description')
    self._AddDescription(dd, policy)
    if 'schema' in policy:
      if self.SchemaHasRangeRestriction(policy['schema']):
        dd = self._AddPolicyAttribute(dl, 'policy_restriction')
        self._AddRangeRestrictionsList(dd, policy['schema'])
    if 'arc_support' in policy:
      dd = self._AddPolicyAttribute(dl, 'arc_support')
      self._AddParagraphs(dd, policy['arc_support'])
    if policy['type'] in ('dict', 'external') and 'schema' in policy:
      self._AddSchema(dl, policy['schema'])
    if 'validation_schema' in policy:
      self._AddSchema(dl, policy['validation_schema'])
    if 'description_schema' in policy:
      self._AddSchema(dl, policy['description_schema'])
    if 'url_schema' in policy:
      dd = self._AddPolicyAttribute(dl, 'url_schema')
      self._AddTextWithLinks(dd, policy['url_schema'])
    if (self.IsPolicySupportedOnWindows(policy) or
        self.IsPolicyOrItemSupportedOnPlatform(policy, 'linux') or
        self.IsPolicyOrItemSupportedOnPlatform(policy, 'android') or
        self.IsPolicyOrItemSupportedOnPlatform(policy, 'mac') or
        self.IsPolicyOrItemSupportedOnPlatform(
            policy, 'chrome_os', management='active_directory')):
      # Don't add an example for Google cloud managed ChromeOS policies.
      dd = self._AddPolicyAttribute(dl, 'example_value')
      self._AddExample(dd, policy)
    if 'atomic_group' in policy:
      dd = self._AddPolicyAttribute(dl, 'policy_atomic_group')
      policy_group_ref = './policy-list-3/atomic_groups'
      if 'local' in self.config and self.config['local']:
        policy_group_ref = './chrome_policy_atomic_groups_list.html'
      self.AddText(dd, self.GetLocalizedMessage('policy_in_atomic_group') + ' ')
      self.AddElement(dd, 'a',
                      {'href': policy_group_ref + '#' + policy['atomic_group']},
                      policy['atomic_group'])

  def _AddPolicyNote(self, parent, policy):
    '''If a policy has an additional web page assigned with it, then add
    a link for that page.

    Args:
      policy: The data structure of the policy.
    '''
    if 'problem_href' not in policy:
      return
    problem_href = policy['problem_href']
    div = self._AddStyledElement(parent, 'div', ['div.note'])
    note = self.GetLocalizedMessage('note').replace('$6', problem_href)
    self._AddParagraphs(div, note)

  def _AddPolicyRow(self, parent, policy):
    '''Adds a row for the policy in the summary table.

    Args:
      parent: The DOM node of the summary table.
      policy: The data structure of the policy.
    '''
    tr = self._AddStyledElement(parent, 'tr', ['tr'])
    indent = 'padding-left: %dpx;' % (7 + self._indent_level * 14)
    if policy['type'] != 'group':
      # Normal policies get two columns with name and caption.
      name_td = self._AddStyledElement(tr, 'td', ['td', 'td.left'],
                                       {'style': indent})
      self.AddElement(name_td, 'a', {'href': '#' + policy['name']},
                      policy['name'])
      self._AddStyledElement(tr, 'td', ['td', 'td.right'], {},
                             policy['caption'])
    else:
      # Groups get one column with caption.
      name_td = self._AddStyledElement(tr, 'td', ['td', 'td.left'], {
          'style': indent,
          'colspan': '2'
      })
      self.AddElement(name_td, 'a', {'href': '#' + policy['name']},
                      policy['caption'])

  def _AddPolicySection(self, parent, policy):
    '''Adds a section about the policy in the detailed policy listing.

    Args:
      parent: The DOM node of the <div> of the detailed policy list.
      policy: The data structure of the policy.
    '''
    # Set style according to group nesting level.
    indent = 'margin-left: %dpx' % (self._indent_level * 28)
    if policy['type'] == 'group':
      heading = 'h2'
    else:
      heading = 'h3'
    parent2 = self.AddElement(parent, 'div', {'style': indent})

    h2 = self.AddElement(parent2, heading)
    self.AddElement(h2, 'a', {'name': policy['name']})
    if policy['type'] != 'group':
      # Normal policies get a full description.
      policy_name_text = policy['name']
      if 'deprecated' in policy and policy['deprecated'] == True:
        policy_name_text += " ("
        policy_name_text += self.GetLocalizedMessage('deprecated') + ")"
      self.AddText(h2, policy_name_text)
      self.AddElement(parent2, 'span', {}, policy['caption'])
      self._AddPolicyNote(parent2, policy)
      self._AddPolicyDetails(parent2, policy)
    else:
      # Groups get a more compact description.
      self.AddText(h2, policy['caption'])
      self._AddStyledElement(parent2, 'div', ['div.group_desc'], {},
                             policy['desc'])
    self.AddElement(parent2, 'a', {'href': '#top'},
                    self.GetLocalizedMessage('back_to_top'))

  def SchemaHasRangeRestriction(self, schema):
    if 'maximum' in schema:
      return True
    if 'minimum' in schema:
      return schema['minimum'] != 0
    return False

  def _BeginTemplate(self, intro_message_id, banner_message_id):
    # Add a <div> for the summary section.
    if self._GetChromiumVersionString() is not None:
      self.AddComment(self._main_div, self.config['build'] + \
          ' version: ' + self._GetChromiumVersionString())

    banner_div = self._AddStyledElement(self._main_div, 'div', ['div.banner'],
                                        {}, '')
    self._AddParagraphs(banner_div, self.GetLocalizedMessage(banner_message_id))
    summary_div = self.AddElement(self._main_div, 'div')
    self.AddElement(summary_div, 'a', {'name': 'top'})
    self.AddElement(summary_div, 'br')
    self._AddParagraphs(summary_div, self.GetLocalizedMessage(intro_message_id))
    self.AddElement(summary_div, 'br')
    self.AddElement(summary_div, 'br')
    self.AddElement(summary_div, 'br')
    # Add the summary table of policies.
    summary_table = self._AddStyledElement(summary_div, 'table', ['table'])
    # Add the first row.
    thead = self.AddElement(summary_table, 'thead')
    tr = self._AddStyledElement(thead, 'tr', ['tr'])
    self._AddStyledElement(tr, 'td', ['td', 'td.left', 'thead td'], {},
                           self.GetLocalizedMessage('name_column_title'))
    self._AddStyledElement(tr, 'td', ['td', 'td.right', 'thead td'], {},
                           self.GetLocalizedMessage('description_column_title'))
    self._summary_tbody = self.AddElement(summary_table, 'tbody')

    # Add a <div> for the detailed policy listing.
    self._details_div = self.AddElement(self._main_div, 'div')

  #
  # Implementation of abstract methods of TemplateWriter:
  #

  def IsDeprecatedPolicySupported(self, policy):
    return True

  def WritePolicy(self, policy):
    self._AddPolicyRow(self._summary_tbody, policy)
    self._AddPolicySection(self._details_div, policy)

  def BeginPolicyGroup(self, group):
    self.WritePolicy(group)
    self._indent_level += 1

  def EndPolicyGroup(self):
    self._indent_level -= 1

  def BeginTemplate(self):
    self._BeginTemplate('intro', 'banner')

  def Init(self):
    dom_impl = minidom.getDOMImplementation('')
    self._doc = dom_impl.createDocument(None, 'html', None)
    body = self.AddElement(self._doc.documentElement, 'body')
    self._main_div = self.AddElement(body, 'div')
    self._indent_level = 0

    # Human-readable names of supported platforms.
    self._PLATFORM_MAP = {
        'win': 'Windows',
        'mac': 'Mac',
        'linux': 'Linux',
        'chrome_os': self.config['os_name'],
        'android': 'Android',
        'win7': 'Windows 7',
        'ios': 'iOS',
    }
    # Human-readable names of supported products.
    self._PRODUCT_MAP = {
        'chrome': self.config['app_name'],
        'chrome_frame': self.config['frame_name'],
        'chrome_os': self.config['os_name'],
        'webview': self.config['webview_name'],
    }
    # Human-readable names of supported features. Each supported feature has
    # a 'doc_feature_X' entry in |self.messages|.
    self._FEATURE_MAP = {}
    for message in self.messages:
      if message.startswith('doc_feature_'):
        self._FEATURE_MAP[message[12:]] = self.messages[message]['text']
    # Human-readable names of types.
    self._TYPE_MAP = {
        'string': 'String',
        'int': 'Integer',
        'main': 'Boolean',
        'int-enum': 'Integer',
        'string-enum': 'String',
        'list': 'List of strings',
        'string-enum-list': 'List of strings',
        'dict': 'Dictionary',
        'external': 'External data reference',
    }
    self._REG_TYPE_MAP = {
        'string': 'REG_SZ',
        'int': 'REG_DWORD',
        'main': 'REG_DWORD',
        'int-enum': 'REG_DWORD',
        'string-enum': 'REG_SZ',
        'dict': 'REG_SZ',
        'external': 'REG_SZ',
    }
    self._RESTRICTION_TYPE_MAP = {
        'int-enum': 'choice',
        'string-enum': 'choice',
        'list': 'string',
        'string-enum-list': 'multi-select',
        'dict': 'string',
        'external': 'string',
    }
    # The CSS style-sheet used for the document. It will be used in Google
    # Sites, which strips class attributes from HTML tags. To work around this,
    # the style-sheet is a dictionary and the style attributes will be added
    # "by hand" for each element.
    self._STYLE = {
        'div.banner': 'background-color: rgb(244,204,204); font-size: x-large; '
                      'border: 1px solid red; padding: 20px; '
                      'text-align: center;',
        'table': 'border-style: none; border-collapse: collapse;',
        'tr': 'height: 0px;',
        'td': 'border: 1px dotted rgb(170, 170, 170); padding: 7px; '
              'vertical-align: top; width: 236px; height: 15px;',
        'thead td': 'font-weight: bold;',
        'td.left': 'width: 200px;',
        'td.right': 'width: 100%;',
        'dt': 'font-weight: bold;',
        'dd dl': 'margin-top: 0px; margin-bottom: 0px;',
        '.monospace': 'font-family: monospace;',
        '.pre-wrap': 'white-space: pre-wrap;',
        'div.note': 'border: 2px solid black; padding: 5px; margin: 5px;',
        'div.group_desc': 'margin-top: 20px; margin-bottom: 20px;',
        'ul': 'padding-left: 0px; margin-left: 0px;'
    }

  def GetTemplateText(self):
    # Return the text representation of the main <div> tag.
    return self._main_div.toxml()
    # To get a complete HTML file, use the following.
    # return self._doc.toxml()
