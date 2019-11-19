# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for sync component.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os
import re

# Some definitions don't follow all the conventions we want to enforce.
# It's either difficult or impossible to fix this, so we ignore the problem(s).
EXCEPTION_MODEL_TYPES = [
  # Grandfathered types:
  'UNSPECIFIED',  # Doesn't have a root tag or notification type.
  'TOP_LEVEL_FOLDER',  # Doesn't have a root tag or notification type.
  'AUTOFILL_WALLET_DATA',  # Root tag and model type string lack DATA suffix.
  'APP_SETTINGS',  # Model type string has inconsistent capitalization.
  'EXTENSION_SETTINGS',  # Model type string has inconsistent capitalization.
  'PROXY_TABS',  # Doesn't have a root tag or notification type.
  'NIGORI',  # Model type string is 'encryption keys'.
  'SUPERVISED_USER_SETTINGS',  # Root tag and model type string replace
                               # 'Supervised' with 'Managed'
  'SUPERVISED_USER_WHITELISTS',  # See previous.

  # Deprecated types:
  'DEPRECATED_EXPERIMENTS']

# Root tags are used as prefixes when creating storage keys, so certain strings
# are blacklisted in order to prevent prefix collision.
BLACKLISTED_ROOT_TAGS = [
  '_mts_schema_descriptor'
]

# Number of distinct fields in a map entry; used to create
# sets that check for uniqueness.
MAP_ENTRY_FIELD_COUNT = 6

# String that precedes the ModelType when referencing the
# proto field number enum e.g.
# sync_pb::EntitySpecifics::kManagedUserFieldNumber.
# Used to map from enum references to the ModelType.
FIELD_NUMBER_PREFIX = 'sync_pb::EntitySpecifics::k'

# Start and end regexes for finding the EntitySpecifics definition in
# sync.proto.
PROTO_DEFINITION_START_PATTERN = '^  oneof specifics_variant \{'
PROTO_DEFINITION_END_PATTERN = '^\}'

# Start and end regexes for finding the ModelTypeInfoMap definition
# in model_type.cc.
MODEL_TYPE_START_PATTERN = '^const ModelTypeInfo kModelTypeInfoMap'
MODEL_TYPE_END_PATTERN = '^\};'

# Strings relating to files we'll need to read.
# model_type.cc is where the ModelTypeInfoMap is
# sync.proto is where the proto definitions for ModelTypes are.
PROTO_FILE_PATH = './protocol/sync.proto'
PROTO_FILE_NAME = 'sync.proto'
MODEL_TYPE_FILE_NAME = 'model_type.cc'

SYNC_SOURCE_FILES = (r'^components[\\/]sync[\\/].*\.(cc|h)$',)

# The wrapper around lint that is called below disables a set of filters if the
# passed filter evaluates to false. Pass a junk filter to avoid this behavior.
LINT_FILTERS = ['+fake/filter']

def CheckModelTypeInfoMap(input_api, output_api, model_type_file):
  """Checks the kModelTypeInfoMap in model_type.cc follows conventions.
  Checks that the kModelTypeInfoMap follows the below rules:
    1) The model type string should match the model type name, but with
       only the first letter capitalized and spaces instead of underscores.
    2) The root tag should be the same as the model type but all lowercase.
    3) The notification type should match the proto message name.
    4) No duplicate data across model types.
   Args:
    input_api: presubmit_support InputApi instance
    output_api: presubmit_support OutputApi instance
    model_type_file: AffectedFile object where the ModelTypeInfoMap is
  Returns:
    A (potentially empty) list PresubmitError objects corresponding to
    violations of the above rules.
  """
  accumulated_problems = []
  map_entries = ParseModelTypeEntries(
    input_api, model_type_file.AbsoluteLocalPath())
  # If any line of the map changed, we check the whole thing since
  # definitions span multiple lines and there are rules that apply across
  # all definitions e.g. no duplicated field values.
  check_map = False
  for line_num, _ in model_type_file.ChangedContents():
    for map_entry in map_entries:
      if line_num in map_entry.affected_lines:
        check_map = True
        break

  if not check_map:
    return []
  proto_field_definitions = ParseSyncProtoFieldIdentifiers(
    input_api, os.path.abspath(PROTO_FILE_PATH))
  accumulated_problems.extend(
    CheckNoDuplicatedFieldValues(output_api, map_entries))

  for map_entry in map_entries:
    entry_problems = []
    entry_problems.extend(
      CheckNotificationTypeMatchesProtoMessageName(
        output_api, map_entry, proto_field_definitions))
    entry_problems.extend(CheckRootTagNotInBlackList(output_api, map_entry))

    if map_entry.model_type not in EXCEPTION_MODEL_TYPES:
      entry_problems.extend(
        CheckModelTypeStringMatchesModelType(output_api, map_entry))
      entry_problems.extend(
        CheckRootTagMatchesModelType(output_api, map_entry))

    if len(entry_problems) > 0:
      accumulated_problems.extend(entry_problems)

  return accumulated_problems


class ModelTypeEnumEntry(object):
  """Class that encapsulates a ModelTypeInfo definition in model_type.cc.
  Allows access to each of the named fields in the definition and also
  which lines the definition spans.
  Attributes:
    model_type: entry's ModelType enum value
    notification_type: model type's notification string
    root_tag: model type's root tag
    model_type_string: string corresponding to the ModelType
    field_number: proto field number
    histogram_val: value identifying ModelType in histogram
    affected_lines: lines in model_type.cc that the definition spans
  """
  def __init__(self, entry_strings, affected_lines):
    (model_type, notification_type, root_tag, model_type_string,
          field_number, histogram_val) = entry_strings
    self.model_type = model_type
    self.notification_type = notification_type
    self.root_tag = root_tag
    self.model_type_string = model_type_string
    self.field_number = field_number
    self.histogram_val = histogram_val
    self.affected_lines = affected_lines


def ParseModelTypeEntries(input_api, model_type_cc_path):
  """Parses model_type_cc_path for ModelTypeEnumEntries
  Args:
    input_api: presubmit_support InputAPI instance
    model_type_cc_path: path to file containing the ModelTypeInfo entries
  Returns:
    A list of ModelTypeEnumEntry objects read from model_type.cc.
    e.g. ('AUTOFILL_WALLET_METADATA', 'WALLET_METADATA',
      'autofill_wallet_metadata', 'Autofill Wallet Metadata',
      'sync_pb::EntitySpecifics::kWalletMetadataFieldNumber', '35',
      [63, 64, 65])
  """
  file_contents = input_api.ReadFile(model_type_cc_path)
  start_pattern = input_api.re.compile(MODEL_TYPE_START_PATTERN)
  end_pattern = input_api.re.compile(MODEL_TYPE_END_PATTERN)
  results, definition_strings, definition_lines = [], [], []
  inside_enum = False
  current_line_number = 0
  for line in file_contents.splitlines():
    current_line_number += 1
    if line.strip().startswith('//'):
      # Ignore comments.
      continue
    if start_pattern.match(line):
      inside_enum = True
      continue
    if inside_enum:
      if end_pattern.match(line):
        break
      line_entries = line.strip().strip('{},').split(',')
      definition_strings.extend([entry.strip('" ') for entry in line_entries])
      definition_lines.append(current_line_number)
      if line.endswith('},'):
        results.append(ModelTypeEnumEntry(definition_strings, definition_lines))
        definition_strings = []
        definition_lines = []
  return results


def ParseSyncProtoFieldIdentifiers(input_api, sync_proto_path):
  """Parses proto field identifiers from the EntitySpecifics definition.
  Args:
    input_api: presubmit_support InputAPI instance
    proto_path: path to the file containing the proto field definitions
  Returns:
    A dictionary of the format {'SyncDataType': 'field_identifier'}
    e.g. {'AutofillSpecifics': 'autofill'}
  """
  proto_field_definitions = {}
  proto_file_contents = input_api.ReadFile(sync_proto_path).splitlines()
  start_pattern = input_api.re.compile(PROTO_DEFINITION_START_PATTERN)
  end_pattern = input_api.re.compile(PROTO_DEFINITION_END_PATTERN)
  in_proto_def = False
  for line in proto_file_contents:
    if start_pattern.match(line):
      in_proto_def = True
      continue
    if in_proto_def:
      if end_pattern.match(line):
        break
      line = line.strip()
      split_proto_line = line.split(' ')
      # ignore comments and lines that don't contain definitions.
      if '//' in line or len(split_proto_line) < 3:
        continue

      field_typename = split_proto_line[0]
      field_identifier = split_proto_line[1]
      proto_field_definitions[field_typename] = field_identifier
  return proto_field_definitions

def StripTrailingS(string):
  return string.rstrip('sS')


def IsTitleCased(string):
  return reduce(lambda bool1, bool2: bool1 and bool2,
    [s[0].isupper() for s in string.split(' ')])


def FormatPresubmitError(output_api, message, affected_lines):
  """ Outputs a formatted error message with filename and line number(s).
  """
  if len(affected_lines) > 1:
    message_including_lines = 'Error at lines %d-%d in model_type.cc: %s' %(
      affected_lines[0], affected_lines[-1], message)
  else:
    message_including_lines = 'Error at line %d in model_type.cc: %s' %(
      affected_lines[0], message)
  return output_api.PresubmitError(message_including_lines)


def CheckNotificationTypeMatchesProtoMessageName(
  output_api, map_entry, proto_field_definitions):
  """Check that map_entry's notification type matches sync.proto.
  Verifies that the notification_type matches the name of the field defined
  in the sync.proto by looking it up in the proto_field_definitions map.
  Args:
    output_api: presubmit_support OutputApi instance
    map_entry: ModelTypeEnumEntry instance
    proto_field_definitions: dict of proto field types and field names
  Returns:
    A potentially empty list of PresubmitError objects corresponding to
    violations of the above rule
  """
  if map_entry.field_number == '-1':
    return []
  proto_message_name = proto_field_definitions[
    FieldNumberToPrototypeString(map_entry.field_number)]
  if map_entry.notification_type.lower() != proto_message_name:
    return [
      FormatPresubmitError(
        output_api,'In the construction of ModelTypeInfo: notification type'
        ' "%s" does not match proto message'
        ' name defined in sync.proto: ' '"%s"' %
        (map_entry.notification_type, proto_message_name),
        map_entry.affected_lines)]
  return []


def CheckNoDuplicatedFieldValues(output_api, map_entries):
  """Check that map_entries has no duplicated field values.
  Verifies that every map_entry in map_entries doesn't have a field value
  used elsewhere in map_entries, ignoring special values ("" and -1).
  Args:
    output_api: presubmit_support OutputApi instance
    map_entries: list of ModelTypeEnumEntry objects to check
  Returns:
    A list PresubmitError objects for each duplicated field value
  """
  problem_list = []
  field_value_sets = [set() for i in range(MAP_ENTRY_FIELD_COUNT)]
  for map_entry in map_entries:
    field_values = [
      map_entry.model_type, map_entry.notification_type,
      map_entry.root_tag, map_entry.model_type_string,
      map_entry.field_number, map_entry.histogram_val]
    for i in range(MAP_ENTRY_FIELD_COUNT):
      field_value = field_values[i]
      field_value_set = field_value_sets[i]
      if field_value in field_value_set:
        problem_list.append(
          FormatPresubmitError(
            output_api, 'Duplicated field value "%s"' % field_value,
            map_entry.affected_lines))
      elif len(field_value) > 0 and field_value != '-1':
        field_value_set.add(field_value)
  return problem_list


def CheckModelTypeStringMatchesModelType(output_api, map_entry):
  """Check that map_entry's model_type_string matches ModelType.
  Args:
    output_api: presubmit_support OutputApi instance
    map_entry: ModelTypeEnumEntry object to check
  Returns:
    A list of PresubmitError objects for each violation
  """
  problem_list = []
  expected_model_type_string = map_entry.model_type.lower().replace('_', ' ')
  if (StripTrailingS(expected_model_type_string) !=
    StripTrailingS(map_entry.model_type_string.lower())):
    problem_list.append(
      FormatPresubmitError(
        output_api,'model type string "%s" does not match model type.'
        ' It should be "%s"' % (
          map_entry.model_type_string, expected_model_type_string.title()),
        map_entry.affected_lines))
  if not IsTitleCased(map_entry.model_type_string):
    problem_list.append(
      FormatPresubmitError(
        output_api,'model type string "%s" should be title cased' %
        (map_entry.model_type_string), map_entry.affected_lines))
  return problem_list


def CheckRootTagMatchesModelType(output_api, map_entry):
  """Check that map_entry's root tag matches ModelType.
  Args:
    output_api: presubmit_support OutputAPI instance
    map_entry: ModelTypeEnumEntry object to check
  Returns:
    A list of PresubmitError objects for each violation
  """
  expected_root_tag = map_entry.model_type.lower()
  if (StripTrailingS(expected_root_tag) !=
    StripTrailingS(map_entry.root_tag)):
    return [
      FormatPresubmitError(
        output_api,'root tag "%s" does not match model type. It should'
        'be "%s"' % (map_entry.root_tag, expected_root_tag),
        map_entry.affected_lines)]
  return []

def CheckRootTagNotInBlackList(output_api, map_entry):
  """ Checks that map_entry's root isn't a blacklisted string.
  Args:
    output_api: presubmit_support OutputAPI instance
    map_entry: ModelTypeEnumEntry object to check
  Returns:
    A list of PresubmitError objects for each violation
  """
  if map_entry.root_tag in BLACKLISTED_ROOT_TAGS:
    return [FormatPresubmitError(
        output_api,'root tag "%s" is a blacklisted root tag'
        % (map_entry.root_tag), map_entry.affected_lines)]
  return []


def FieldNumberToPrototypeString(field_number):
  """Converts a field number enum reference to an EntitySpecifics string.
  Converts a reference to the field number enum to the corresponding
  proto data type string.
  Args:
    field_number: string representation of a field number enum reference
  Returns:
    A string that is the corresponding proto field data type. e.g.
    FieldNumberToPrototypeString('EntitySpecifics::kAppFieldNumber')
    => 'AppSpecifics'
  """
  return field_number.replace(FIELD_NUMBER_PREFIX, '').replace(
    'FieldNumber', 'Specifics')

def CheckChangeLintsClean(input_api, output_api):
  source_filter = lambda x: input_api.FilterSourceFile(
    x, white_list=SYNC_SOURCE_FILES, black_list=None)
  return input_api.canned_checks.CheckChangeLintsClean(
      input_api, output_api, source_filter, lint_filters=LINT_FILTERS,
      verbose_level=1)

def CheckChanges(input_api, output_api):
  results = []
  results += CheckChangeLintsClean(input_api, output_api)
  for f in input_api.AffectedFiles():
    if (f.LocalPath().endswith(MODEL_TYPE_FILE_NAME) or
        f.LocalPath().endswith(PROTO_FILE_NAME)):
      results += CheckModelTypeInfoMap(input_api, output_api, f)
  return results

def CheckChangeOnUpload(input_api, output_api):
  return CheckChanges(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChanges(input_api, output_api)
