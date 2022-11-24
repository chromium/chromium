# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import re

# Dict with valid schema type names as keys. The values are the allowed
# attribute names and their expected value types.
#
# These are the ONLY supported schema features. For the full schema proposal see
# https://json-schema.org/understanding-json-schema/index.html.
#
# There are also these departures from the proposal:
#   - "additionalProperties": false is not supported. Instead, it is assumed by
#     default. The value of "additionalProperties" has to be a schema.
ALLOWED_ATTRIBUTES_AND_TYPES = {
    'boolean': {
        'type': str,  # required
        'id': str,  # optional
        'description': str,  # optional
        'sensitiveValue': bool  # optional
    },
    'string': {
        'type': str,  # required
        'id': str,  # optional
        'description': str,  # optional
        'enum': list,  # optional
        'pattern': str,  # optional
        'sensitiveValue': bool  # optional
    },
    'integer': {
        'type': str,  # required
        'id': str,  # optional
        'description': str,  # optional
        'enum': list,  # optional
        'minimum': int,  # optional
        'maximum': int,  # optional
        'sensitiveValue': bool  # optional
    },
    'array': {
        'type': str,  # required
        'id': str,  # optional
        'items': dict,  # required,
        'description': str,  # optional
        'sensitiveValue': bool  # optional
    },
    'object': {
        'type': str,  # required
        'id': str,  # optional
        'description': str,  # optional
        'properties': dict,  #           one of these 3 properties is required
        'patternProperties': dict,  #    one of these 3 properties is required
        'additionalProperties': dict,  # one of these 3 properties is required
        'required': list,  # optional
        'sensitiveValue': bool  # optional
    }
}

# Dict of allowed attributes and their expected types for schemas with a $ref.
ALLOWED_REF_ATTRIBUTES_AND_TYPES = {'$ref': str, 'description': str}

# Dict of human-readable enum types and their python types as values.
ENUM_ITEM_TYPES = {'integer': int, 'string': str}


class SchemaValidator(object):
  """This class can validate schemas by calling ValidateSchema() or can
  validate values against their schema by calling ValidateValue().
  Schemas with an 'id' used as $ref link by another schema have to be passed to
  ValidateSchema() before the schema with the $ref can be used in
  ValidateSchema() or ValidateValue().
  |schemas_by_id| is used as storage for schemas with their 'id' as key and
  their schemas as value.
  """

  def __init__(self):
    self.schemas_by_id = {}
    self.invalid_ref_ids = set()
    self.found_ref_ids = set()
    self.errors = []
    self.enforce_use_entire_schema = False
    self.expected_properties = {}
    self.expected_pattern_properties = {}
    self.expected_additional_properties = {}
    self.used_properties = {}
    self.used_pattern_properties = {}
    self.used_additional_properties = {}

  def ValidateSchema(self, schema, schemas_by_id):
    """Checks if |schema| is a valid schema and only uses valid $ref links.

    See _ValidateSchemaInternal() for a detailed description of the schema
    validation. This method also checks if all used $ref links are known and
    valid.
    Args:
      schema (dict): The JSON schema.
      schemas_by_id (dict): A dictionary of all the schemas that can refered to
                            using $ref.
    Returns:
      A list contains all schema errors.
    """
    self.found_ref_ids.clear()
    self.errors = []
    self.schemas_by_id = schemas_by_id

    self._ValidateSchemaInternal(schema)

    unknown_ref_ids = self.found_ref_ids.difference(self.schemas_by_id.keys())
    for unknown_ref_id in unknown_ref_ids:
      if unknown_ref_id in self.invalid_ref_ids:
        self._Error("$ref to invalid schema '%s'." % unknown_ref_id)
      else:
        self._Error("Unknown $ref '%s'." % unknown_ref_id)

    return self.errors

  def _ValidateSchemaInternal(self, schema):
    """Check if |schema| is a valid schema.

    This method checks whether |schema| is a dict, has a valid 'type' property,
    only has properties and values allowed by its type (see
    ALLOWED_ATTRIBUTES_AND_TYPES) and calls the appropriate
    _Validate{Integer,String,Array,Object}Schema() method.
    If the schema has a $ref, the $ref id is added to |found_ref_ids| so that
    its existence can be validated later on in ValidateSchema(). They may
    also not contain other attributes except for '$ref' and 'description' (see
    ALLOWED_REF_ATTRIBUTES_AND_TYPES).
    If the schema has an id, the id has to be unique and the schema is stored
    for later re-use if it is valid.
    Args:
      schema (dict): The JSON schema.
    """
    num_errors_before = len(self.errors)
    # Check that schema is of type dict.
    if not isinstance(schema, dict):
      self._Error("Schema must be a dict.")

    # Validate $ref links. All '$ref' links are gathered in |found_ref_links| so
    # that their existence can be validated later on in ValidateSchema(schema).
    if '$ref' in schema:
      ref_id = schema['$ref']
      for name, value in schema.items():
        if name not in ALLOWED_REF_ATTRIBUTES_AND_TYPES:
          self._Error("Attribute '%s' is not allowed for schema with $ref '%s'."
                      % (name, ref_id))
        expected_type = ALLOWED_REF_ATTRIBUTES_AND_TYPES[name]
        if not isinstance(value, expected_type):
          self._Error(
              ("Attribute value for '%s' (%s) has incorrect type (Expected "
               "type: '%s'; actual type: '%s')") % (name, value, expected_type,
                                                    type(value)))
      self.found_ref_ids.add(ref_id)
      return

    # Every schema (non-ref) must have a type.
    if 'type' not in schema:
      self._Error("Missing attribute 'type'.")
    schema_type = schema['type']

    # Check that the type is valid.
    if schema_type not in ALLOWED_ATTRIBUTES_AND_TYPES:
      self._Error("Unknown type: %s" % schema_type)

    # Check that each schema only contains attributes that are allowed in their
    # respective type and that their values are of correct type.
    allowed_attributes = ALLOWED_ATTRIBUTES_AND_TYPES[schema_type]
    for attribute_name, attribute_value in schema.items():
      if attribute_name in allowed_attributes:
        expected_type = allowed_attributes[attribute_name]
        if not isinstance(attribute_value, expected_type):
          self._Error(
              ("Attribute '%s' has incorrect type (Expected type: '%s'; actual "
               "type: '%s').") % (attribute_name, expected_type,
                                  type(attribute_value)))
      else:
        self._Error("Attribute '%s' is not allowed for type '%s'." %
                    (attribute_name, schema_type))

    # Validate schemas depending on 'type'.
    if schema_type == 'string':
      self._ValidateStringSchema(schema)
    elif schema_type == 'integer':
      self._ValidateIntegerSchema(schema)
    elif schema_type == 'array':
      self._ValidateArraySchema(schema)
    elif schema_type == 'object':
      self._ValidateObjectSchema(schema)

    # If the schema has an 'id', ensure that the id is unique and store the
    # schema for later reference.
    if 'id' in schema:
      ref_id = schema['id']
      if ref_id in self.schemas_by_id:
        self._Error("ID '%s' is not unique." % ref_id)
      if len(self.errors) == num_errors_before:
        self.schemas_by_id[ref_id] = schema
      else:
        self.invalid_ref_ids.add(ref_id)

  def _ValidateStringSchema(self, schema):
    """Validates a |schema| with type 'string'.

    Validates the 'enum' (see _ValidateEnum()) and/or 'pattern' property (see
    _ValidatePattern()) if existing.
    Args:
      schema (dict): The JSON schema.
    """
    if 'enum' in schema:
      self._ValidateEnum(schema['enum'], 'string')
    if 'pattern' in schema:
      self._ValidatePattern(schema['pattern'])

  def _ValidateIntegerSchema(self, schema):
    """Validates a |schema| with type 'integer'.

    Validates the 'enum' property (see _ValidateEnum()) if existing. This
    method also ensures that the specified minimum value is smaller or equal to
    the specified maximum value, if both exist.
    Args:
      schema (dict): The JSON schema.
    """
    if 'enum' in schema:
      self._ValidateEnum(schema['enum'], 'integer')
    if ('minimum' in schema and 'maximum' in schema and
        schema['minimum'] > schema['maximum']):
      self._Error("Invalid range specified: [%s; %s]" % (schema['minimum'],
                                                         schema['maximum']))

  def _ValidateArraySchema(self, schema):
    """Validates a |schema| with type 'array'.

    Validates that the 'items' attribute exists and its value is a valid schema.
    Args:
      schema (dict): The JSON schema.
    """
    if 'items' in schema:
      self._ValidateSchemaInternal(schema['items'])
    else:
      self._Error("Schema of type 'array' must have an 'items' attribute.")

  def _ValidateObjectSchema(self, schema):
    """Validates a schema of type 'object'.

    If |schema| has a 'required' attribute, this method validates that it is not
    empty, only contains strings and only contains property names of properties
    defined in the 'properties' attribute.
    This method also ensures that at least one of 'properties',
    'patternProperties' or 'additionalProperties' is defined.
    If 'properties' are defined, they must have non-empty string names and
    contain a valid schema.
    If 'patternProperties' are defined, they must be valid regex patterns and
    contain a valid schema.
    If 'additionalProperties is defined, it must contain a valid schema.
    Args:
      schema (dict): The JSON schema.
    '"""
    # Validate 'required' attribute.
    if 'required' in schema:
      required_properties = schema['required']
      if not required_properties:
        self._Error("Attribute 'required' may not be empty (omit it if empty).")
      if not all(
          isinstance(required_property, str)
          for required_property in required_properties):
        self._Error("Attribute 'required' may only contain strings.")
      properties = schema.get('properties', {})
      unknown_properties = [
          property_name for property_name in required_properties
          if property_name not in properties
      ]
      if unknown_properties:
        self._Error("Unknown properties in 'required': %s" % unknown_properties)
    # Validate '*properties' attributes.
    has_any_properties = False
    if 'properties' in schema:
      has_any_properties = True
      properties = schema['properties']
      for property_name, property_schema in properties.items():
        if not isinstance(property_name, str):
          self._Error("Property name must be a string.")
        if not property_name:
          self._Error("Property name may not be empty.")
        self._ValidateSchemaInternal(property_schema)
    if 'patternProperties' in schema:
      has_any_properties = True
      pattern_properties = schema['patternProperties']
      for property_pattern, property_schema in pattern_properties.items():
        self._ValidatePattern(property_pattern)
        self._ValidateSchemaInternal(property_schema)
    if 'additionalProperties' in schema:
      has_any_properties = True
      additional_properties = schema['additionalProperties']
      self._ValidateSchemaInternal(additional_properties)
    if not has_any_properties:
      self._Error(
          "Schema of type 'object' must have at least one of the following "
          "attributes: ['properties', 'patternProperties' or "
          "'additionalProperties'].")

  def _ValidateEnum(self, enum, schema_type):
    """Validates an |enum| of type |schema_type|.

    Validates that |enum| is not empty and its elements have the correct type
    according to |schema_type| (see ENUM_ITEM_TYPES).
    Args:
      enum (list): The list of enum values.
      schema_type (str): The schema type in which the enum is used.
    """
    if not enum:
      self._Error("Attribute 'enum' may not be empty.")
    item_type = ENUM_ITEM_TYPES[schema_type]
    if not all(isinstance(enum_value, item_type) for enum_value in enum):
      self._Error(("Attribute 'enum' for type '%s' may only contain elements of"
                   " type %s: %s") % (schema_type, item_type, enum))

  def _ValidatePattern(self, pattern):
    """Validates a regex |pattern|.

    Validates that |pattern| is a string and can be used as regex pattern.
    Args:
      pattern (str): The regex pattern.
    """
    if not isinstance(pattern, str):
      self._Error("Pattern must be a string: %s" % pattern)
    try:
      re.compile(pattern)
    except re.error:
      self._Error("Pattern is not a valid regex: %s" % pattern)

  def ValidateValue(self, schema, value, enforce_use_entire_schema=False):
    """Validates that |value| complies to |schema|.

    See _ValidateValueInternal(schema, value) for a detailed description of the
    value validation. If |enforce_use_entire_schema| is enabled, each value and
    its sub-values have to use every property of each used schema at least once
    and values with schema type 'array' may not be empty.
    Args:
      schema (dict): The JSON schema.
      value (any): The value being validated.
      enforce_use_entire_schema (bool): Whether each property hsa to be used at
        least once.
    Returns:
      A list contains all value errors.
    """
    self.enforce_use_entire_schema = enforce_use_entire_schema
    self.expected_properties = {}
    self.expected_pattern_properties = {}
    self.expected_additional_properties = {}
    self.used_properties = {}
    self.used_pattern_properties = {}
    self.used_additional_properties = {}
    self.errors = []

    self._ValidateValueInternal(schema, value)

    # Check that all properties, patternProperties and additionalProperties were
    # used at least once for each schema.
    if self.enforce_use_entire_schema:
      if self.expected_properties != self.used_properties:
        for schema_id, expected_properties \
            in self.expected_properties.items():
          used_properties = self.used_properties.get(schema_id, set())
          unused_properties = expected_properties.difference(used_properties)
          if unused_properties:
            self._Error("Unused properties: %s" % unused_properties)
      if self.expected_pattern_properties != self.used_pattern_properties:
        for schema_id, expected_properties \
            in self.expected_pattern_properties.items():
          used_properties = self.used_pattern_properties.get(schema_id, set())
          unused_properties = expected_properties.difference(used_properties)
          if unused_properties:
            self._Error("Unused pattern properties: %s" % unused_properties)
      if self.expected_additional_properties != self.used_additional_properties:
        self._Error("Unused additional properties.")

    return self.errors

  def _ValidateValueInternal(self, schema, value):
    """Validates that |value| complies to |schema|.

    This method checks if the |value|'s type is correct according to type
    expected in |schema| and calls the associated
    _Validate{Integer,String,Array,Object}ValueInternal(schema, value).
    Args:
      schema (dict): The JSON schema.
      value (any): The value being validated.
    """
    # Load schema from store if it has '$ref'.
    if '$ref' in schema:
      ref_id = schema['$ref']
      if ref_id not in self.schemas_by_id:
        self._Error("Unknown $ref id: %s" % ref_id)
      schema = self.schemas_by_id[ref_id]

    schema_type = schema.get('type')
    if schema_type == 'boolean' and isinstance(value, bool):
      pass  # Boolean doesn't need any validation.
    elif schema_type == 'integer' and isinstance(value, int):
      self.ValidateIntegerValue(schema, value)
    elif schema_type == 'string' and isinstance(value, (bytes, str)):
      self.ValidateStringValue(schema, value)
    elif schema_type == 'array' and isinstance(value, list):
      self.ValidateArrayValue(schema, value)
    elif schema_type == 'object' and isinstance(value, dict):
      self.ValidateObjectValue(schema, value)
    else:
      # Type mismatch or unknown type.
      self._Error(
          "Type mismatch or unknown (schema_type: %s; value_type: %s): %s" %
          (schema_type, type(value), value))

  def ValidateIntegerValue(self, schema, value):
    """Validates an integer |value| according to |schema|.

    If the |schema| has an enum of possible values, check whether |value| is one
    of them.
    If 'minimum' and/or 'maximum' are defined in |schema|, check that
    minimum <= value <= maximum.
    Args:
      schema (dict): The JSON schema.
      value (int): The value being validated.
    """
    if 'enum' in schema:
      self._ValidateEnumValue(schema['enum'], value)
    if (('minimum' in schema and value < schema['minimum']) or
        ('maximum' in schema and value > schema['maximum'])):
      self._Error(
          "Value %s not in range [%s,%s]." %
          (value, schema.get('minimum', '-inf'), schema.get('maximum', '+inf')))

  def ValidateStringValue(self, schema, value):
    """Validates a string |value| according to |schema|.

    If the |schema| has an enum of possible values, check whether |value| is one
    of them.
    If the |schema| has a 'pattern' attribute, check whether |value| matches the
    pattern.
    Args:
      schema (dict): The JSON schema.
      value (str): The value being validated.
    """
    if 'enum' in schema:
      self._ValidateEnumValue(schema['enum'], value)
    if 'pattern' in schema:
      pattern = schema['pattern']
      if not re.search(pattern, value):
        self._Error(
            "String value '%s' does not match pattern '%s'." % (value, pattern))

  def ValidateArrayValue(self, schema, child_values):
    """Validates an array |child_values| according to |schema|.

    Validates each item in |child_values| (see _ValidateValueInternal()).
    If |enforce_use_entire_schema| is enabled, the value must contain at least
    one element.
    Args:
      schema (dict): The JSON schema.
      child_values (list): The list of children being validated.
    """
    child_schema = schema.get('items')
    for child_value in child_values:
      self._ValidateValueInternal(child_schema, child_value)
    if self.enforce_use_entire_schema and not child_values:
      self._Error("Array must contain at least one item.")

  def ValidateObjectValue(self, schema, value):
    """Validates an object |value| according to |schema|.

    Validates each property in |value| according to its matching schema out of
    the |schema|'s 'properties', 'patternProperties' or 'additionalProperties'
    properties. Also adds the property to the set of |used_*properties|.
    If the |schema| is used for the first time in this validation, the
    |expected_*properties| are initialized to the |schema|'s properties.
    Args:
      schema (dict): The JSON schema.
      value (dict): The value being validated.
    """
    # Get allowed properties.
    properties = schema.get('properties', {})
    pattern_properties = schema.get('patternProperties', {})
    additional_properties = schema.get('additionalProperties', {})

    # If the schema hasn't been used before, store sets of expected properties,
    # patternProperties and a bool whether we expect additionalProperties to be
    # used. Also initialize the list of used properties and patternProperties to
    # empty sets.
    schema_id = id(schema)
    if schema_id not in self.expected_properties:
      self.expected_properties[schema_id] = set(properties.keys())
      self.expected_pattern_properties[schema_id] = set(
          pattern_properties.keys())
      self.expected_additional_properties[schema_id] = (
          'additionalProperties' in schema)
      self.used_properties[schema_id] = set()
      self.used_pattern_properties[schema_id] = set()
      self.used_additional_properties[schema_id] = False

    for property_key, property_value in value.items():
      # Find property schema from either properties, patternProperties or
      # additionalProperties.
      property_schema = {}
      if properties and property_key in properties:
        property_schema = properties[property_key]
        self.used_properties[schema_id].add(property_key)
      elif pattern_properties:
        matched_pattern = next((pattern
                                for pattern in pattern_properties.keys()
                                if re.search(pattern, property_key)), "")
        property_schema = pattern_properties.get(matched_pattern, {})
        self.used_pattern_properties[schema_id].add(matched_pattern)
      if not property_schema and additional_properties:
        property_schema = additional_properties
        self.used_additional_properties[schema_id] = True

      if not property_schema:
        self._Error("Unknown property: %s" % property_key)
      self._ValidateValueInternal(property_schema, property_value)

    # Check that all 'required' properties are existing.
    if 'required' in schema:
      missing_required = [
          required_key for required_key in schema['required']
          if required_key not in value
      ]
      if missing_required:
        self._Error("Required property missing: %s" % missing_required)

  def _ValidateEnumValue(self, enum, value):
    """Validates that |value| is in |enum|.

    Args:
      enum (list): The list of allowed values.
      value (any): The value being validated.
    """
    if value not in enum:
      self._Error("Unknown enum value: %s (expected one of %s)" % (value, enum))

  def _Error(self, message):
    """Captures an error.

    Stores error |messages|.
    Args:
      message (str): The error message."""
    self.errors.append(message)
