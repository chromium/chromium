#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from xml.dom import minidom
from writers import gpo_editor_writer, xml_formatted_writer


class AdmxElementType:
  '''The different types of ADMX elements that can be used to display a policy.
  This is related to the 'type' field in policy_templates.json, but there isn't
  a perfect 1:1 mapping. This class is also used when writing ADML files, to
  ensure that the ADML generated from policy_templates.json is compatible with
  the ADMX generated from policy_templates.json"""
  '''
  MAIN = 1
  STRING = 2
  MULTI_STRING = 3
  INT = 4
  ENUM = 5
  LIST = 6
  GROUP = 7

  @staticmethod
  def GetType(policy, allow_multi_strings=False):
    '''Returns the ADMX element type that should be used for the given policy.
    This logic is shared between the ADMX writer and the ADML writer, to ensure
    that the ADMX and ADML generated from policy_tempates.json are compatible.

    Args:
      policy: A dict describing the policy, as found in policy_templates.json.
      allow_multi_strings: If true, the use of multi-line textbox elements is
          allowed, so this function will sometimes return MULTI_STRING. If false
          it falls back to single-line textboxes instead by returning STRING.

    Returns:
      One of the enum values of AdmxElementType.

    Raises:
      Exception: If policy['type'] is not recognized.
    '''
    policy_type = policy['type']
    policy_example = policy.get('example_value')

    # TODO(olsen): Some policies are defined in policy_templates.json as type
    # string, but the string is actually a JSON object. We should change the
    # schema so they are 'dict' or similar, but until then, we use this
    # heuristic to decide whether they are actually JSON and so could benefit
    # from being displayed to the user as a multi-line string:
    if (policy_type == 'string' and allow_multi_strings and
        policy_example is not None and policy_example.strip().startswith('{')):
      return AdmxElementType.MULTI_STRING

    admx_element_type = AdmxElementType._POLICY_TYPE_MAP.get(policy_type)
    if admx_element_type is None:
      raise Exception('Unknown policy type %s.' % policy_type)

    if (admx_element_type == AdmxElementType.MULTI_STRING and
        not allow_multi_strings):
      return AdmxElementType.STRING

    return admx_element_type


AdmxElementType._POLICY_TYPE_MAP = {
    'main': AdmxElementType.MAIN,
    'string': AdmxElementType.STRING,
    'dict': AdmxElementType.MULTI_STRING,
    'external': AdmxElementType.MULTI_STRING,
    'int': AdmxElementType.INT,
    'int-enum': AdmxElementType.ENUM,
    'string-enum': AdmxElementType.ENUM,
    'list': AdmxElementType.LIST,
    'string-enum-list': AdmxElementType.LIST,
    'group': AdmxElementType.GROUP
}


def GetWriter(config):
  '''Factory method for instanciating the ADMXWriter. Every Writer needs a
  GetWriter method because the TemplateFormatter uses this method to
  instantiate a Writer.
  '''
  return ADMXWriter(['win', 'win7'], config)


class ADMXWriter(xml_formatted_writer.XMLFormattedWriter,
                 gpo_editor_writer.GpoEditorWriter):
  '''Class for generating an ADMX policy template. It is used by the
  PolicyTemplateGenerator to write the admx file.
  '''

  # DOM root node of the generated ADMX document.
  _doc = None

  # The ADMX "policies" element that contains the ADMX "policy" elements that
  # are generated.
  _active_policies_elem = None

  def Init(self):
    # Shortcut to platform-specific ADMX/ADM specific configuration.
    assert len(self.platforms) <= 2
    self.winconfig = self.config['win_config'][self.platforms[0]]

  def _AdmlString(self, name):
    '''Creates a reference to the named string in an ADML file.
    Args:
      name: Name of the referenced ADML string.
    '''
    name = name.replace('.', '_')
    return '$(string.' + name + ')'

  def _AdmlStringExplain(self, name):
    '''Creates a reference to the named explanation string in an ADML file.
    Args:
      name: Name of the referenced ADML explanation.
    '''
    name = name.replace('.', '_')
    return '$(string.' + name + '_Explain)'

  def _AdmlPresentation(self, name):
    '''Creates a reference to the named presentation element in an ADML file.
    Args:
      name: Name of the referenced ADML presentation element.
    '''
    return '$(presentation.' + name + ')'

  def _AddPolicyNamespaces(self, parent, prefix, namespace):
    '''Generates the ADMX "policyNamespace" element and adds the elements to the
    passed parent element. The namespace of the generated ADMX document is
    define via the ADMX "target" element. Used namespaces are declared with an
    ADMX "using" element. ADMX "target" and "using" elements are children of the
    ADMX "policyNamespace" element.

    Args:
      parent: The parent node to which all generated elements are added.
      prefix: A logical name that can be used in the generated ADMX document to
        refere to this namespace.
      namespace: Namespace of the generated ADMX document.
    '''
    policy_namespaces_elem = self.AddElement(parent, 'policyNamespaces')
    attributes = {
        'prefix': prefix,
        'namespace': namespace,
    }
    self.AddElement(policy_namespaces_elem, 'target', attributes)
    if 'admx_using_namespaces' in self.config:
      prefix_namespace_map = self.config['admx_using_namespaces']
      for prefix in prefix_namespace_map:
        attributes = {
            'prefix': prefix,
            'namespace': prefix_namespace_map[prefix],
        }
        self.AddElement(policy_namespaces_elem, 'using', attributes)
    attributes = {
        'prefix': 'windows',
        'namespace': 'Microsoft.Policies.Windows',
    }
    self.AddElement(policy_namespaces_elem, 'using', attributes)

  def _AddCategory(self, parent, name, display_name, parent_category_name=None):
    '''Adds an ADMX category element to the passed parent node. The following
    snippet shows an example of a category element where "chromium" is the value
    of the parameter name:

    <category displayName="$(string.chromium)" name="chromium"/>

    Each parent node can have only one category with a given name. Adding the
    same category again with the same attributes is ignored, but adding it
    again with different attributes is an error.

    Args:
      parent: The parent node to which all generated elements are added.
      name: Name of the category.
      display_name: Display name of the category.
      parent_category_name: Name of the parent category. Defaults to None.
    '''
    existing = list(
        filter(lambda e: e.getAttribute('name') == name,
               parent.getElementsByTagName('category')))
    if existing:
      assert len(existing) == 1
      assert existing[0].getAttribute('name') == name
      assert existing[0].getAttribute('displayName') == display_name
      return
    attributes = {
        'name': name,
        'displayName': display_name,
    }
    category_elem = self.AddElement(parent, 'category', attributes)
    if parent_category_name:
      attributes = {'ref': parent_category_name}
      self.AddElement(category_elem, 'parentCategory', attributes)

  def _AddCategories(self, categories):
    '''Generates the ADMX "categories" element and adds it to the categories
    main node. The "categories" element defines the category for the policies
    defined in this ADMX document. Here is an example of an ADMX "categories"
    element:

    <categories>
      <category displayName="$(string.googlechrome)" name="googlechrome">
        <parentCategory ref="Google:Cat_Google"/>
      </category>
    </categories>

    Args:
      categories_path: The categories path e.g. ['google', 'googlechrome']. For
        each level in the path a "category" element will be generated, unless
        the level contains a ':', in which case it is treated as external
        references and no element is generated. Except for the root level, each
        level refers to its parent. Since the root level category has no parent
        it does not require a parent reference.
    '''
    category_name = None
    for category in categories:
      parent_category_name = category_name
      category_name = category
      if (":" not in category_name):
        self._AddCategory(self._categories_elem, category_name,
                          self._AdmlString(category_name), parent_category_name)

  def _AddSupportedOn(self, parent, supported_os_list):
    '''Generates the "supportedOn" ADMX element and adds it to the passed
    parent node. The "supportedOn" element contains information about supported
    Windows OS versions. The following code snippet contains an example of a
    "supportedOn" element:

    <supportedOn>
      <definitions>
        <definition name="$(supported_os)"
                    displayName="$(string.$(supported_os))"/>
        </definitions>
        ...
    </supportedOn>

    Args:
      parent: The parent element to which all generated elements are added.
      supported_os: List with all supported Win OSes.
    '''
    supported_on_elem = self.AddElement(parent, 'supportedOn')
    definitions_elem = self.AddElement(supported_on_elem, 'definitions')
    for supported_os in supported_os_list:
      attributes = {
          'name': supported_os,
          'displayName': self._AdmlString(supported_os)
      }
      self.AddElement(definitions_elem, 'definition', attributes)

  def _AddStringPolicy(self, parent, name, id=None):
    '''Generates ADMX elements for a String-Policy and adds them to the
    passed parent node.
    '''
    attributes = {
        'id': id or name,
        'valueName': name,
        'maxLength': '1000000',
    }
    self.AddElement(parent, 'text', attributes)

  def _AddMultiStringPolicy(self, parent, name):
    '''Generates ADMX elements for a multi-line String-Policy and adds them to
    the passed parent node.
    '''
    # We currently also show a single-line textbox - see http://crbug/829328
    self._AddStringPolicy(parent, name, id=name + '_Legacy')
    attributes = {
        'id': name,
        'valueName': name,
        'maxLength': '1000000',
    }
    self.AddElement(parent, 'multiText', attributes)

  def _AddIntPolicy(self, parent, policy):
    '''Generates ADMX elements for an Int-Policy and adds them to the passed
    parent node.
    '''
    #default max value for an integer
    max = 2000000000
    min = 0
    if self.PolicyHasRestrictions(policy):
      schema = policy['schema']
      if 'minimum' in schema and schema['minimum'] >= 0:
        min = schema['minimum']
      if 'maximum' in schema and schema['maximum'] >= 0:
        max = schema['maximum']
    assert type(min) == int
    assert type(max) == int
    attributes = {
        'id': policy['name'],
        'valueName': policy['name'],
        'maxValue': str(max),
        'minValue': str(min),
    }
    self.AddElement(parent, 'decimal', attributes)

  def _AddEnumPolicy(self, parent, policy):
    '''Generates ADMX elements for an Enum-Policy and adds them to the
    passed parent element.
    '''
    name = policy['name']
    items = policy['items']
    attributes = {
        'id': name,
        'valueName': name,
    }
    enum_elem = self.AddElement(parent, 'enum', attributes)
    for item in items:
      attributes = {'displayName': self._AdmlString(name + "_" + item['name'])}
      item_elem = self.AddElement(enum_elem, 'item', attributes)
      value_elem = self.AddElement(item_elem, 'value')
      value_string = str(item['value'])
      if policy['type'] == 'int-enum':
        self.AddElement(value_elem, 'decimal', {'value': value_string})
      else:
        self.AddElement(value_elem, 'string', {}, value_string)

  def _AddListPolicy(self, parent, key, name):
    '''Generates ADMX XML elements for a List-Policy and adds them to the
    passed parent element.
    '''
    attributes = {
        # The ID must be in sync with ID of the corresponding element in the
        # ADML file.
        'id': name + 'Desc',
        'valuePrefix': '',
        'key': key + '\\' + name,
    }
    self.AddElement(parent, 'list', attributes)

  def _AddMainPolicy(self, parent):
    '''Generates ADMX elements for a Main-Policy amd adds them to the
    passed parent element.
    '''
    enabled_value_elem = self.AddElement(parent, 'enabledValue')
    self.AddElement(enabled_value_elem, 'decimal', {'value': '1'})
    disabled_value_elem = self.AddElement(parent, 'disabledValue')
    self.AddElement(disabled_value_elem, 'decimal', {'value': '0'})

  def PolicyHasRestrictions(self, policy):
    if 'schema' in policy:
      return any(keyword in policy['schema'] \
        for keyword in ['minimum', 'maximum'])
    return False

  def _GetElements(self, policy_group_elem):
    '''Returns the ADMX "elements" child from an ADMX "policy" element. If the
    "policy" element has no "elements" child yet, a new child is created.

    Args:
      policy_group_elem: The ADMX "policy" element from which the child element
        "elements" is returned.

    Raises:
      Exception: The policy_group_elem does not contain a ADMX "policy" element.
    '''
    if policy_group_elem.tagName != 'policy':
      raise Exception('Expected a "policy" element but got a "%s" element' %
                      policy_group_elem.tagName)
    elements_list = policy_group_elem.getElementsByTagName('elements')
    if len(elements_list) == 0:
      return self.AddElement(policy_group_elem, 'elements')
    elif len(elements_list) == 1:
      return elements_list[0]
    else:
      raise Exception('There is supposed to be only one "elements" node but'
                      ' there are %s.' % str(len(elements_list)))

  def _GetAdmxElementType(self, policy):
    '''Returns the ADMX element type for a particular Policy.'''
    return AdmxElementType.GetType(policy, allow_multi_strings=False)

  def _WritePolicy(self, policy, name, key, parent):
    '''Generates ADMX elements for a Policy.'''
    policies_elem = self._active_policies_elem
    policy_name = policy['name']
    attributes = {
        'name': name,
        'class': self.GetClass(policy),
        'displayName': self._AdmlString(policy_name),
        'explainText': self._AdmlStringExplain(policy_name),
        'presentation': self._AdmlPresentation(policy_name),
        'key': key,
    }
    is_win7_only = self.IsPolicyOnWin7Only(policy)
    supported_key = ('win_supported_os_win7'
                     if is_win7_only else 'win_supported_os')
    supported_on_text = self.config[supported_key]

    # Store the current "policy" AMDX element in self for later use by the
    # WritePolicy method.
    policy_elem = self.AddElement(policies_elem, 'policy', attributes)
    self.AddElement(policy_elem, 'parentCategory', {'ref': parent})
    self.AddElement(policy_elem, 'supportedOn', {'ref': supported_on_text})

    element_type = self._GetAdmxElementType(policy)
    if element_type == AdmxElementType.MAIN:
      self.AddAttribute(policy_elem, 'valueName', policy_name)
      self._AddMainPolicy(policy_elem)
    elif element_type == AdmxElementType.STRING:
      parent = self._GetElements(policy_elem)
      self._AddStringPolicy(parent, policy_name)
    elif element_type == AdmxElementType.MULTI_STRING:
      parent = self._GetElements(policy_elem)
      self._AddMultiStringPolicy(parent, policy_name)
    elif element_type == AdmxElementType.INT:
      parent = self._GetElements(policy_elem)
      self._AddIntPolicy(parent, policy)
    elif element_type == AdmxElementType.ENUM:
      parent = self._GetElements(policy_elem)
      self._AddEnumPolicy(parent, policy)
    elif element_type == AdmxElementType.LIST:
      parent = self._GetElements(policy_elem)
      self._AddListPolicy(parent, key, policy_name)
    elif element_type == AdmxElementType.GROUP:
      pass
    else:
      raise Exception('Unknown element type %s.' % element_type)

  def WritePolicy(self, policy):
    if self.CanBeMandatory(policy):
      self._WritePolicy(policy, policy['name'],
                        self.winconfig['reg_mandatory_key_name'],
                        self._active_mandatory_policy_group_name)

  def WriteRecommendedPolicy(self, policy):
    self._WritePolicy(policy, policy['name'] + '_recommended',
                      self.winconfig['reg_recommended_key_name'],
                      self._active_recommended_policy_group_name)

  def _BeginPolicyGroup(self, group, name, parent):
    '''Generates ADMX elements for a Policy-Group.
    '''
    attributes = {
        'name': name,
        'displayName': self._AdmlString(group['name'] + '_group'),
    }
    category_elem = self.AddElement(self._categories_elem, 'category',
                                    attributes)
    attributes = {'ref': parent}
    self.AddElement(category_elem, 'parentCategory', attributes)

  def BeginPolicyGroup(self, group):
    self._BeginPolicyGroup(group, group['name'],
                           self.winconfig['mandatory_category_path'][-1])
    self._active_mandatory_policy_group_name = group['name']

  def EndPolicyGroup(self):
    self._active_mandatory_policy_group_name = \
        self.winconfig['mandatory_category_path'][-1]

  def BeginRecommendedPolicyGroup(self, group):
    self._BeginPolicyGroup(group, group['name'] + '_recommended',
                           self.winconfig['recommended_category_path'][-1])
    self._active_recommended_policy_group_name = group['name'] + '_recommended'

  def EndRecommendedPolicyGroup(self):
    self._active_recommended_policy_group_name = \
        self.winconfig['recommended_category_path'][-1]

  def BeginTemplate(self):
    '''Generates the skeleton of the ADMX template. An ADMX template contains
    an ADMX "PolicyDefinitions" element with four child nodes: "policies"
    "policyNamspaces", "resources", "supportedOn" and "categories"
    '''
    dom_impl = minidom.getDOMImplementation('')
    self._doc = dom_impl.createDocument(None, 'policyDefinitions', None)
    if self._GetChromiumVersionString() is not None:
      self.AddComment(self._doc.documentElement, self.config['build'] + \
          ' version: ' + self._GetChromiumVersionString())
    policy_definitions_elem = self._doc.documentElement

    policy_definitions_elem.attributes['revision'] = '1.0'
    policy_definitions_elem.attributes['schemaVersion'] = '1.0'

    self._AddPolicyNamespaces(policy_definitions_elem,
                              self.config['admx_prefix'],
                              self.winconfig['namespace'])
    self.AddElement(policy_definitions_elem, 'resources',
                    {'minRequiredRevision': '1.0'})
    self._AddSupportedOn(
        policy_definitions_elem,
        [self.config['win_supported_os'], self.config['win_supported_os_win7']])
    self._categories_elem = self.AddElement(policy_definitions_elem,
                                            'categories')
    self._AddCategories(self.winconfig['mandatory_category_path'])
    self._AddCategories(self.winconfig['recommended_category_path'])
    self._active_policies_elem = self.AddElement(policy_definitions_elem,
                                                 'policies')
    self._active_mandatory_policy_group_name = \
        self.winconfig['mandatory_category_path'][-1]
    self._active_recommended_policy_group_name = \
        self.winconfig['recommended_category_path'][-1]

  def GetTemplateText(self):
    return self.ToPrettyXml(self._doc)

  def GetClass(self, policy):
    return 'Both'
