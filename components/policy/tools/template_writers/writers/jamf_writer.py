#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import json

from writers import template_writer


def GetWriter(config):
  '''Factory method for creating JamfWriter objects.
  See the constructor of TemplateWriter for description of
  arguments.
  '''
  return JamfWriter(['mac', 'ios'], config)


class JamfWriter(template_writer.TemplateWriter):
  '''Simple writer that writes a jamf.json file.
  '''
  MAX_RECURSIVE_FIELDS_DEPTH = 5
  TYPE_TO_INPUT = {
      'string': 'string',
      'int': 'integer',
      'int-enum': 'integer',
      'string-enum': 'string',
      'string-enum-list': 'array',
      'main': 'boolean',
      'list': 'array',
      'dict': 'object',
      'external': 'object',
  }

  # Some policies are forced to a certain schema, so they bypass TYPE_TO_INPUT
  POLICY_ID_TO_INPUT = {
      227: 'string',  # ManagedBookmarks
      278: 'string',  # ExtensionSettings
  }


  def WriteTemplate(self, template):
    '''Writes the given template definition.

    Args:
      template: Template definition to write.

    Returns:
      Generated output for the passed template definition.
    '''
    self.messages = template['messages']
    # Keep track of all items that can be referred to by an id.
    # This is used for '$ref' fields in the policy templates.
    ref_ids_schemas = {}
    policies = []
    for policy_def in template['policy_definitions']:
      # Iterate over all policies, even the policies contained inside a policy
      # group.
      if policy_def['type'] == 'group':
        policies += policy_def['policies']
      else:
        policies += [policy_def]

    for policy in policies:
      if policy['type'] == 'int-enum' or policy['type'] == 'string-enum':
        self.RecordEnumIds(policy, ref_ids_schemas)
      elif 'schema' in policy:
        if 'id' in policy['schema']:
          self.RecordKnownPropertyIds(policy['schema'], ref_ids_schemas)
        if 'items' in policy['schema']:
          self.RecordKnownPropertyIds(policy['schema']['items'],
                                      ref_ids_schemas)
        if 'properties' in policy['schema']:
          self.RecordKnownPropertyIds(policy['schema']['properties'],
                                      ref_ids_schemas)
        if 'patternProperties' in policy['schema']:
          self.RecordKnownPropertyIds(policy['schema']['patternProperties'],
                                      ref_ids_schemas)

    policies = [policy for policy in policies if self.IsPolicySupported(policy)]
    output = {
        'title': self.config['bundle_id'],
        'version': self.config['version'].split(".", 1)[0],
        'description': self.config['app_name'],
        'options': {
            'remove_empty_properties': True
        },
        'properties': {}
    }

    for policy in policies:
      output['properties'][policy['name']] = {
          'title':
          policy['name'],
          'description':
          policy['caption'],
          'type':
          self.TYPE_TO_INPUT[policy['type']],
          'links': [{
              'rel': self.messages['doc_policy_documentation']['text'],
              'href': self.config['doc_url'] + '#' + policy['name']
          }]
      }

      policy_output = output['properties'][policy['name']]
      if policy['id'] in self.POLICY_ID_TO_INPUT:
        policy_output['type'] = self.POLICY_ID_TO_INPUT[policy['id']]

      policy_type = policy_output['type']
      if policy['type'] == 'int-enum' or policy['type'] == 'string-enum':
        policy_output['options'] = {
            'enum_titles': [item['name'] for item in policy['items']]
        }
        policy_output['enum'] = [item['value'] for item in policy['items']]
      elif policy['type'] == 'int' and 'schema' in policy:
        if 'minimum' in policy['schema']:
          policy_output['minimum'] = policy['schema']['minimum']
        if 'maximum' in policy['schema']:
          policy_output['maximum'] = policy['schema']['maximum']
      elif policy['type'] == 'list':
        policy_output['items'] = policy['schema']['items']
      elif policy['type'] == 'string-enum-list' or policy[
          'type'] == 'int-enum-list':
        policy_output['items'] = {
            'type': policy['schema']['items']['type'],
            'options': {
                'enum_titles': [item['name'] for item in policy['items']]
            },
            'enum': [item['value'] for item in policy['items']]
        }
      elif policy_output['type'] == 'object' and policy['type'] != 'external':
        policy_output['type'] = policy['schema']['type']
        if policy_output['type'] == 'array':
          policy_output['items'] = policy['schema']['items']
          self.WriteRefItems(policy_output['items'], policy_output['items'], [],
                             ref_ids_schemas, set())
        elif policy_output['type'] == 'object':
          policy_output['properties'] = policy['schema']['properties']
          self.WriteRefItems(policy_output['properties'],
                             policy_output['properties'], [], ref_ids_schemas,
                             set())

    return json.dumps(output, indent=2, sort_keys=True, separators=(',', ': '))

  def RecordEnumIds(self, policy, known_ids):
    '''Writes the a dictionary mapping ids of enums that can be referred to by
       '$ref' to their schema.

    Args:
      policy: The policy to scan for refs.
      known_ids: The dictionary and output of all the known ids.
    '''
    if 'id' in policy['schema']:
      known_ids[policy['schema']['id']] = {
          'type': policy['schema']['type'],
          'options': {
              'enum_titles': [item['name'] for item in policy['items']]
          },
          'enum': [item['value'] for item in policy['items']]
      }

  def RecordKnownPropertyIds(self, obj, known_ids):
    '''Writes the a dictionary mapping ids of schemas properties that can be
       referred to by '$ref' to their schema.

    Args:
      obj: The object to scan for refs.
      known_ids: The dictionary and output of all the known ids.
    '''
    if type(obj) is not dict:
      return
    if 'id' in obj:
      known_ids[obj['id']] = obj
    for value in obj.values():
      self.RecordKnownPropertyIds(value, known_ids)

  def WriteRefItems(self, root, obj, path_to_obj_parent, known_ids,
                    ids_in_ancestry):
    '''Replaces all the '$ref' items by their actual value. Nested properties
      are limited to a depth of MAX_RECURSIVE_FIELDS_DEPTH, after which the
      recursive field is removed.

    Args:
      root: The root of the object tree to scan for refs.
      obj: The current object being checked for ids.
      path_to_obj_parent: A array of all the keys leading to the parent of |obj|
                          starting at |root|.
      known_ids: The dictionary of all the known ids.
      ids_in_ancestry: A list of ids found in the tree starting at root. Use to
                       keep nested fields in check.
    '''
    if type(obj) is not dict:
      return
    if 'id' in obj:
      ids_in_ancestry.add(obj['id'])
    # Make a copy of items since we are going to change |obj|.
    for key, value in list(obj.items()):
      if type(value) is not dict:
        continue
      if '$ref' in value:
        # If the id is an ancestor, we have a nested field.
        if value['$ref'] in ids_in_ancestry:
          id = value['$ref']

          last_obj = None
          parent = root
          grandparent = root

          # Find the parent and grandparent of obj to create the |last_obj|
          # which is the field where the nesting stops.
          for i in range(0, len(path_to_obj_parent)):
            if i + 1 < len(path_to_obj_parent):
              grandparent = grandparent[path_to_obj_parent[i]]
            else:
              parent = grandparent[path_to_obj_parent[i]]
              # Remove the link between grand parent and parent so we can have a
              # copy of the object without nesting.
              grandparent[path_to_obj_parent[i]] = None
              del grandparent[path_to_obj_parent[i]]
              # last_obj is a copy of the reference object without nesting.
              last_obj = copy.deepcopy(known_ids[id])
              # Re-establish the link between grand parent and parent.
              grandparent[path_to_obj_parent[i]] = parent

          del obj[key]
          obj[key] = last_obj
          # Create nested '$ref' objects with |last_obj| as the last object.
          for count in range(1, self.MAX_RECURSIVE_FIELDS_DEPTH):
            obj[key] = copy.deepcopy(known_ids[id])
          obj_grandparent_ref = path_to_obj_parent[len(path_to_obj_parent) - 1]
        else:
          # If no nested field, simply assign the '$ref'.
          obj[key] = dict(known_ids[value['$ref']])
          self.WriteRefItems(root, obj[key], path_to_obj_parent + [key],
                             known_ids, ids_in_ancestry)
      else:
        self.WriteRefItems(root, value, path_to_obj_parent + [key], known_ids,
                           ids_in_ancestry)
