# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parser for the Autofill AI entity schema.

See parse_entity_schema().
"""

import collections
import io
import itertools
import os
import sys
_FILE_PATH = os.path.dirname(os.path.realpath(__file__))
_JSON5_PATH = os.path.join(
    _FILE_PATH,
    os.pardir,
    os.pardir,
    os.pardir,
    os.pardir,
    os.pardir,
    os.pardir,
    'third_party',
    'pyjson5',
    'src',
)
sys.path.append(_JSON5_PATH)
import json5

_REQUIRED_KEYS = {
    'name',
    'attributes',
    'obfuscated attributes',
    'required fields',
    'import constraints',
    'merge constraints',
    'strike keys',
    'disambiguation order',
    'read only',
}
_OPTIONAL_KEYS = {'experiment feature', 'excluded geo-ips'}
_CONSTRAINTS_KEYS = {
    'import constraints',
    'merge constraints',
    'strike keys',
    'required fields',
}


class SchemaValidationError(Exception):
  """Base error for all parsing issues."""

  def __init__(self, entity, message):
    name = entity.get('name', '<unnamed>')
    super().__init__(f'Autofill AI schema: entity "{name}": {message}')


def _resolve_shorthands(schema):
  """Resolves shorthands in the given schema.

  For brevity, the schema allows shorthands:
  - Constraints in the JSON object can refer to each other, e.g.,
    { "import constraints":  [ ["foo", "bar"], ["qux"] ],
      "merge constraints":   "import constraints" }
    expands to
    { "import constraints":  [ ["foo", "bar"], ["qux"] ],
      "merge constraints":   [ ["foo", "bar"], ["qux"] ] }
  - Constraints can use keywords:
    { "import constraints":  "any",
      "merge constraints":   "all" }
    expands to
    { "import constraints":  [ ["foo"], ["bar"], ["qux"] ],
      "merge constraints":   [ ["foo", "bar", "qux"] ] }

  Args:
    schema: The schema, which is mutated.
  """
  for entity in schema:
    constraints_keys = [key for key in _CONSTRAINTS_KEYS if key in entity]

    # Constraints can be the shorthands 'all' (= all attributes) or 'any' (= at
    # least one attribute):
    for key in constraints_keys:
      if entity[key] == 'all':
        entity[key] = [[attribute for attribute in entity['attributes']]]
      if entity[key] == 'any':
        entity[key] = [[attribute] for attribute in entity['attributes']]

    # Constraints can refer to one another.
    for lhs, rhs in itertools.product(constraints_keys, constraints_keys):
      if entity[lhs] == rhs and isinstance(entity[rhs], list):
        entity[lhs] = entity[rhs]


def _validate_attributes(entity, attributes, allow_empty):
  """Validates a list of attributes of an entity.

  Args:
    entity: The entity to which the attributes belong.
    attributes: The list of attributes to validate.
    allow_empty: Whether or not an empty list of attributes is an error.

  Yields:
    Error messages.
  """
  if not isinstance(attributes, list):
    yield 'is not a list'
  if not attributes and not allow_empty:
    yield f'is an empty list'

  duplicate_attributes = {
      attribute
      for attribute, count in collections.Counter(attributes).items()
      if count > 1
  }
  for attribute in duplicate_attributes:
    yield f'contains a duplicate attribute {attribute}'

  unknown_attributes = set(attributes) - set(entity['attributes'])
  for attribute in unknown_attributes:
    yield f'contains an unknown attribute "{attribute}"'


def _validate_entity(entity):
  """Validates a single entity.

  Args:
    entity: The entity dictionary.

  Yields:
    Error messages.
  """
  missing_keys = _REQUIRED_KEYS - entity.keys()
  for key in missing_keys:
    yield f'missing key "{key}"'
  if missing_keys:
    return  # We'd hit Python errors if we continue with this entity.

  unknown_keys = entity.keys() - (_REQUIRED_KEYS | _OPTIONAL_KEYS)
  for key in unknown_keys:
    yield f'unknown key "{key}"'

  if not isinstance(entity['name'], str):
    yield '"name": value is not a string'

  if not entity['name']:
    yield '"name": value is the empty string'

  known_attributes = set(entity['attributes'])
  for attribute in known_attributes:
    if not isinstance(attribute, str):
      yield f'attribute {attribute} is not a string'
    if not attribute:
      yield f'attribute {attribute} is empty'

  yield from (
      f'"attributes" {msg}'
      for msg in _validate_attributes(
          entity, entity['attributes'], allow_empty=False
      )
  )
  yield from (
      f'"obfuscated attributes" {msg}'
      for msg in _validate_attributes(
          entity, entity['obfuscated attributes'], allow_empty=True
      )
  )
  yield from (
      f'"disambiguation order" {msg}'
      for msg in _validate_attributes(
          entity, entity['disambiguation order'], allow_empty=True
      )
  )
  for constraints_key in _CONSTRAINTS_KEYS:
    for constraint in entity[constraints_key]:
      yield from (
          f'"{constraints_key}": some constraint {msg}'
          for msg in _validate_attributes(entity, constraint, allow_empty=False)
      )

  if not isinstance(entity['read only'], bool):
    yield '"read only": value is not a Boolean'

  if entity['read only']:
    if entity['import constraints']:
      yield '"import constraints": value must be empty if "read only" is true'
    if entity['merge constraints']:
      yield '"merge constraints": value must be empty if "read only" is true'
    if entity['strike keys']:
      yield '"strike keys": value must be empty if "read only" is true'

  if not isinstance(entity.get('experiment feature', ''), str):
    yield '"experiment feature": value is not a string'


def _validate_schema(schema):
  """Validates all entities of a schema.

  Args:
    schema: The schema dictionary (parsed from JSON).

  Raises:
    SchemaValidationError: The schema is invalid. May raise a group.
  """
  errors = [
      SchemaValidationError(entity, msg)
      for entity in schema
      for msg in _validate_entity(entity)
  ]
  if errors:
    raise errors[0] if len(errors) == 1 else ExceptionGroup(
        'Multiple parsing errors', errors
    )


def parse_entity_schema(path_to_schema_json):
  """Builds the Autofill AI entity schema.

  Reads and returns the JSON schema after resolving shorthands and validating
  the schema.

  Args:
    path_to_schema_json: The path to the JSON schema.

  Returns:
    The parsed schema as dictionary.

  Raises:
    SchemaValidationError: The schema is invalid. May raise a group.
    OSError: The schema cannot be opened and decoded.
  """
  with io.open(path_to_schema_json, 'r', encoding='utf-8') as input_handle:
    schema = json5.load(input_handle)
  _resolve_shorthands(schema)
  _validate_schema(schema)
  return schema
