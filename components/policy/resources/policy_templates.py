#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import glob
import json
import copy
import os
import sys

# This script runs in Chromium and ChromiumOS.
try:
  # Chromium.
  _SRC_PATH = os.path.abspath(
      os.path.join(os.path.dirname(__file__), '..', '..', '..'))
  sys.path.append(os.path.join(_SRC_PATH, 'third_party'))
  import pyyaml
except ImportError:
  # ChromiumOS.
  # Some consumers (ChromiumOS ebuilds) of this script have pyyaml already in
  # their environment and may not have access to the full Chromium repository
  # nor the exact file structure. Import based on the Chromium third_party may
  # not always be possible.
  # Those consumers may install pyyaml in their environment and import it with
  # the name of the module instead as per the library documentation.
  # For compatibility reasons, please refer to
  # https://source.chromium.org/chromium/chromium/src/+/main:third_party/pyyaml/README.chromium
  # to know which version of pyyaml to use.
  import yaml as pyyaml

def _SafeListDir(directory):
  '''Wrapper around os.listdir() that ignores files created by Finder.app.'''
  # On macOS, Finder.app creates .DS_Store files when a user visit a
  # directory causing failure of the script laters on because there
  # are no such group as .DS_Store. Skip the file to prevent the error.
  return filter(lambda name:(name != '.DS_Store'),sorted(os.listdir(directory)))

TEMPLATES_PATH =  os.path.join(
  os.path.dirname(__file__), 'templates')

DEFAULT_TEMPLATES_GEN_PATH =  os.path.join(
  os.path.dirname(__file__), 'policy_templates.json')

POLICY_DEFINITIONS_KEY = 'policy_definitions'


def _SubstituteSchemaRefNames(node, child_key, common_schema, parent_refs,
                              refs_seen):
  '''Converts recursively objects with the key '$ref' into their actual schema.
  '''

  if (not isinstance(node, dict) or not child_key in node
    or not isinstance(node[child_key], dict)):
    return
  if '$ref' in node[child_key]:
    ref_name = node[child_key]['$ref']
    # If the parent has the same id as child, leave the child's ref to avoid
    # infinite loop.
    if ref_name not in parent_refs:
      node[child_key] = copy.deepcopy(common_schema[ref_name])
      parent_refs.add(ref_name)
    # If the schema has been seen already, only keep a reference to the first to
    # avoid having the same ref defined at multiple places.
    if ref_name not in refs_seen:
      node[child_key]['id'] = ref_name
    refs_seen.add(ref_name)

  for ck in sorted(node[child_key].keys()):
    # Copy parents ref so that parents are unique for each child branch and do
    # not mix with sibling nodes.
    _SubstituteSchemaRefNames(node[child_key], ck, common_schema,
                              parent_refs.copy(), refs_seen)


def _SubstituteSchemaRefs(policies, common_schema):
  '''Converts objects with the key '$ref' into their actual schema.

    Args:
      policies: List of policies.
      common_schema: Dictionary of schemas by their ref names.'''
  policy_list = [policy for policy in policies if 'schema' in policy]

  refs_seen = set()
  for policy in sorted(policy_list, key=lambda policy: policy['id']):
    parent_refs = set()
    _SubstituteSchemaRefNames(policy, 'schema', common_schema, parent_refs,
                              refs_seen)
    parent_refs = set()
    _SubstituteSchemaRefNames(policy, 'validation_schema', common_schema,
                              parent_refs, refs_seen)


def _BuildPolicyTemplate(data):
  '''
  Converts data into the format of the old policy_templates.json so that it
  can be used in policy_templates.grd

  Schema : {
    "policy_definitions": {
      "type": "list",
      "items": {
        "id": { "type": "number" },
        "name": "string",
        "policies": { "type": "list", "items": "string" } // for policy groups.
        // See components/policy/resources/new_policy_templates/policy.yaml
        // for the other variables/
      }
    },
    "messages": {
      "type": "Object",
      "patternProperties": {
        "[A-Za-z]": { "desc": "string", "text": "String" }
      }
    },
    //components/policy/resources/templates/manual_device_policy_proto_map.yaml
    // Includes policies where generate_device_proto is true.
    "device_policy_proto_map": {
      "type": "Object",
      "patternProperties": {
        "[A-Za-z]": { "type": "Object", "properties": "String" }
      }
    },
    // Only lists of 2 items
    "legacy_device_policy_proto_map": { "type": "list", "items": "String" },
    //components/policy/resources/templates/messages.yaml
    "messages": {
      "type": "Object",
      "patternProperties": {
        "[A-Za-z]": {
          "type": "Object",
          "properties": { "text": string, "description": "string" }
      }
    },
    "risk_tag_definitions": {
      "type": "list",
      "items": {
        "name": "string",
        "description": "string",
        "user-description": "string"
      }
    },
    "policy_atomic_group_definitions": {
      "type": "list",
      "items": {
        "id": { "type": "number" },
        "name": "string",
        "caption": "string",
        "policies": { "type": "list", "items": "string" } // for policy groups.
        // See components/policy/resources/new_policy_templates/policy.yaml
        // for the other variables/
      }
    },
    "placeholders": { "type": "list" },
    "deleted_policy_ids": { "type": "list", "items": { "type": "number" } },
    "deleted_atomic_policy_group_ids": {
      "type": "list",
      "items": { "type": "number" }
    },
    "highest_id_currently_used": {  "type": "number" },
    "highest_atomic_group_id_currently_used": {  "type": "number" },
  }
  '''

  policy_name_id = { name: id for id, name
    in data['policies']['policies'].items() if name }
  atomic_group_name_id = { name: id
    for id, name in data['policies']['atomic_groups'].items() if name }

  policy_groups = [{
        'name': group_name,
        'type': 'group',
        'caption': group['caption'],
        'desc': group['desc'],
        'policies': list(group['policies'])
    } for group_name, group in data[POLICY_DEFINITIONS_KEY].items()]

  policies = []
  atomic_groups = []
  for group in data[POLICY_DEFINITIONS_KEY].values():
    for policy_name, policy in group['policies'].items():
      policies.append({
        'id': policy_name_id[policy_name],
        'name': policy_name, **policy
      })

    for name, atomic_group in group['policy_atomic_groups'].items():
      atomic_groups.append({
        'id': atomic_group_name_id[name],
        'name': name, **atomic_group
      })

  device_policy_proto_map = data['manual_device_policy_proto_map'].copy()

  for policy in policies:
    if not policy.get('device_only', False):
      continue

    if not policy.get('generate_device_proto', True):
      continue

    device_policy_proto_map[policy['name']] = policy['name'] + '.value'

  result = {
      POLICY_DEFINITIONS_KEY: policies + policy_groups,
      'deleted_policy_ids':
      [id for id, name in data['policies']['policies'].items() if not name],
      'highest_id_currently_used': len(data['policies']['policies']),
      'policy_atomic_group_definitions': atomic_groups,
      'deleted_atomic_policy_group_ids': [
          id for id, name in data['policies']['atomic_groups'].items()
          if not name
      ],
      'highest_atomic_group_id_currently_used':
      len(data['policies']['atomic_groups']),
      'placeholders': [],
      'legacy_device_policy_proto_map': [],
      'device_policy_proto_map': device_policy_proto_map,
      'messages': data['messages'],
      'risk_tag_definitions': [{'name': name, **value}
        for name, value in data['risk_tag_definitions'].items()]
  }
  for key, values in data['legacy_device_policy_proto_map'].items():
    for item in values:
      result['legacy_device_policy_proto_map'].append([key, item])

  _SubstituteSchemaRefs(result[POLICY_DEFINITIONS_KEY], data['common_schemas'])

  return result


def _GetMetadata():
  '''Returns an object containing the policy metadata in order to build the
     policy definition template.'''
  result = {}
  for file in _SafeListDir(TEMPLATES_PATH):
    filename = os.fsdecode(file)
    file_basename, file_extension = os.path.splitext(filename)
    if not file_extension == ".yaml":
      continue
    with open(os.path.join(TEMPLATES_PATH, filename), encoding='utf-8') as f:
      result[file_basename] = pyyaml.safe_load(f)
  return result


def _GetPoliciesAndGroups():
  '''Returns an object containing the policy groups with their details, policies
     and atomic policy groups in order to build the policy definition template.
  '''
  result = {}
  policy_definitions_path = os.path.join(TEMPLATES_PATH, POLICY_DEFINITIONS_KEY)
  for group_name in _SafeListDir(policy_definitions_path):
    result[group_name] = {'policies': {}, 'policy_atomic_groups': {}}
    group_path = os.path.join(policy_definitions_path, group_name)
    if not os.path.isdir(group_path):
      continue

    for file in _SafeListDir(group_path):
      filename = os.fsdecode(file)
      file_basename, file_extension = os.path.splitext(filename)
      file_path = os.path.join(group_path, filename)

      if file_extension != '.yaml':
        continue

      with open(file_path, encoding='utf-8') as f:
        data = pyyaml.safe_load(f)
      if file_basename == '.group.details':
        result[group_name].update(data)
      elif file_basename == 'policy_atomic_groups':
        result[group_name]['policy_atomic_groups'].update(data)
      else:
        result[group_name]['policies'][file_basename] = data
  return result

def _LoadPolicies():
  '''
  Loads all the yaml files used to define policies and their metadata into a
  single object. This is a direct representation of the data structure of the
  directories and their files.

  Schema : {
    //components/policy/resources/templates/policies.yaml
    "policies":{
      // Map of policy atomic group ID to policy atomic group name.
      "atomic_groups": {
        "type": "Object",
        "patternProperties": {
          "[A-Za-z]": { "type": "Object", "properties": "String" }
        }
      }
      // Map of policy ID to policy name.
      "policies": {
        "type": "Object",
        "patternProperties": {
          "[A-Za-z]": { "type": "Object", "properties": "String" }
        }
      }
    }
    "policy_definitions": {
        "type": "Object",
        "patternProperties": {
          // Dictionary of policy groups
          "[A-Za-z]": {
            "type": "Object",
            // c/policy/resources/new_policy_templates/.groups.details.yaml
            "properties": {
              "caption": {"type": "string"},
              "description": {"type": "string"}
            },
            "patternProperties": {
              // Dictionary of policies
              "[A-Za-z]": {
                "type": "Object",
                "properties": {
                  // c/policy/resources/new_policy_templates/policy.yaml
                }
              }
            }
          }
        }
      }
    },
    // components/policy/resources/templates/manual_device_policy_proto_map.yaml
    "manual_device_policy_proto_map": {
      "type": "Object",
      "patternProperties": {
        "[A-Za-z]": { "type": "Object", "properties": "String" }
      }
    },
    // components/policy/resources/templates/legacy_device_policy_proto_map.yaml
    "legacy_device_policy_proto_map": {
      "type": "Object",
      "patternProperties": {
        "[A-Za-z]": { "type": "list", "items": "String" }
      }
    },
    // components/policy/resources/templates/messages.yaml
    "messages": {
      "type": "Object",
      "patternProperties": {
        "[A-Za-z]": {
          "type": "Object",
          "properties": { "text": string, "description": "string" }
        }
      }
    },
    // components/policy/resources/templates/risk_tag_definitions.yaml
    "risk_tag_definitions": {
      "type": "Object",
      "patternProperties": {
        "[A-Za-z]": {
          "type": "Object",
          "properties": { "description": string, "user-description": "string" }
        }
      }
    }
  }
  '''
  return {
    POLICY_DEFINITIONS_KEY: _GetPoliciesAndGroups(),
    **_GetMetadata()
  }

def GetPolicyTemplates():
  '''Returns an object containing the policy templates.
  '''
  template = _LoadPolicies()
  return _BuildPolicyTemplate(template)


def _WriteDepFile(dep_file, target, source_files):
  '''Writes a dep file for `target` at `dep_file` with `source_files` as the
     dependencies.

    Args:
      dep_file: A path to the dependencies file for this script.
      target: The build target.
      source_files: A list of the dependencies for the build target.
  '''
  with open(dep_file, "w") as f:
    f.write(target)
    f.write(": ")
    f.write(' '.join(source_files))


def main():
  '''Generates the a JSON file at `dest` with all the policy definitions.
     If `dest` is not specified, a file name 'policy_templates.json' will be
     generated in the same directory as the script.

    Args:
      dest: A path to the policy templates generated definitions.
      depfile: A path to the dependencies file for this script.
  '''
  parser = argparse.ArgumentParser()
  parser.add_argument('--dest', dest='dest')
  parser.add_argument('--depfile', dest='deps_file')

  args = parser.parse_args()
  if args.dest:
    path = os.path.join(args.dest)
  else:
    path = DEFAULT_TEMPLATES_GEN_PATH

  policy_templates = GetPolicyTemplates()
  with open(path, 'w+', encoding='utf-8') as dest:
    json.dump(policy_templates, dest, indent=2, sort_keys=True)

  files = sorted([f.replace('\\', '/')
    for f in glob.glob(TEMPLATES_PATH + '/**/*.yaml', recursive=True)])

  if args.deps_file:
    _WriteDepFile(args.deps_file, args.dest, files)

if '__main__' == __name__:
  sys.exit(main())
