#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This tool provides a user-friendly way to create policy configurations for
testing. It takes a simple JSON file defining user and/or device policies
and converts it into the serialized and encoded protobuf format that
fake_dmserver can serve.
"""

import argparse
import base64
import json
import logging
import sys

# The ebuild installs these to /usr/local/share/policy-test-tool
POLICY_TEST_TOOL_PATH = "/usr/local/share/policy-test-tool"
sys.path.insert(0, POLICY_TEST_TOOL_PATH)

logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')

try:
  import chrome_device_policy_pb2
  import chrome_settings_pb2
  import policy_common_definitions_pb2
except ImportError as e:
  logging.critical(f"Failed to import Chrome policy protobuf modules: {e}")
  sys.exit(1)


def generate_device_policy_schema(manual_map_path):
  """Builds a schema for device policies by parsing the manual map."""
  schema = {}
  try:
    with open(manual_map_path, "r", encoding="utf-8") as f:
      for line in f:
        line = line.strip()
        if not line or line.startswith("#"):
          continue
        parts = line.split(":", 1)
        if len(parts) == 2:
          policy_name = parts[0].strip()
          proto_path = parts[1].strip().split(" #")[0].strip()
          schema[policy_name] = proto_path
        else:
          raise ValueError(f"Malformed line in manual map file: '{line}'. "
                           "Expected format 'policy_name: proto_path'.")
  except IOError as e:
    raise IOError(
        f"Could not read manual map file at '{manual_map_path}': {e}") from e
  return schema


def populate_message_from_dict(message, data_dict):
  """Recursively populates a protobuf message from a dictionary."""
  for key, value in data_dict.items():
    field_descriptor = message.DESCRIPTOR.fields_by_name.get(key)
    if not field_descriptor:
      logging.warning(f"Field '{key}' not found in protobuf message "
                      f"{message.DESCRIPTOR.name}")
      continue

    if field_descriptor.type == field_descriptor.TYPE_MESSAGE:
      nested_message = getattr(message, key)
      if field_descriptor.label == field_descriptor.LABEL_REPEATED:
        for sub_dict in value:
          new_item = nested_message.add()
          populate_message_from_dict(new_item, sub_dict)
      else:
        populate_message_from_dict(nested_message, value)
    elif field_descriptor.type == field_descriptor.TYPE_ENUM:
      upper_value = value.upper()
      enum_value = None
      for enum_name in field_descriptor.enum_type.values_by_name:
        if enum_name.endswith(upper_value):
          enum_value = field_descriptor.enum_type.values_by_name[enum_name]
          break

      if enum_value is not None:
        setattr(message, key, enum_value.number)
      else:
        logging.warning(f"Enum value '{value}' not found for field '{key}'")
    else:
      setattr(message, key, value)


def apply_user_policies(policies, settings):
  """Applies user policies to the ChromeSettingsProto."""
  for key, value in policies.items():
    if not hasattr(settings, key):
      logging.warning(f"Policy '{key}' not found in protobuf schema. Skipping.")
      continue

    wrapper_message = getattr(settings, key)
    try:
      if isinstance(value, list):
        list_field = getattr(wrapper_message, key)
        list_field.entries.extend(value)
      elif isinstance(value, dict):
        setattr(wrapper_message, key, json.dumps(value))
      else:  # Simple types
        setattr(wrapper_message, key, value)

      wrapper_message.policy_options.mode = (
          policy_common_definitions_pb2.PolicyOptions.MANDATORY)
    except (AttributeError, TypeError) as e:
      raise ValueError(f"Error setting user policy '{key}': {e}") from e


def apply_device_policies(policies, settings, schema):
  """Applies device policies using the schema from the manual map."""
  for key, value in policies.items():
    proto_path = schema.get(key)
    if not proto_path:
      raise ValueError(f"Device policy '{key}' not found in schema.")

    try:
      parts = proto_path.split(".")
      message = settings
      for part in parts[:-1]:
        message = getattr(message, part)

      final_field = parts[-1]
      if isinstance(value, list) and value and isinstance(value[0], dict):
        repeated_field = getattr(message, final_field)
        for item_dict in value:
          new_item = repeated_field.add()
          populate_message_from_dict(new_item, item_dict)
      elif isinstance(value, list):
        getattr(message, final_field).extend(value)
      elif isinstance(value, dict):
        setattr(message, final_field, json.dumps(value))
      else:
        setattr(message, final_field, value)
    except (AttributeError, TypeError, ValueError) as e:
      raise ValueError(f"Error setting device policy '{key}' with path "
                       f"'{proto_path}': {e}") from e


def main():
  """Main script execution."""

  parser = argparse.ArgumentParser(
      description="Generates a policy blob for fake_dmserver from a simple "
      "JSON file.\n\nThis tool is for advanced/manual usage. It converts a "
      "user-friendly JSON\nfile into the serialized protobuf format that "
      "fake_dmserver requires.",
      epilog="""For detailed usage instructions, including the manual workflow,
please refer to the README.md in this directory.""",
      formatter_class=argparse.RawTextHelpFormatter)

  parser.add_argument(
      "--input-policies",
      required=True,
      help="Path to the simple JSON file defining policies.",
  )

  parser.add_argument(
      "--output-blob",
      required=True,
      help="Path to write the final fake_dmserver policy.json blob.",
  )

  parser.add_argument(
      "--manual-map",
      required=True,
      help="Path to the manual_device_policy_proto_map.yaml file.",
  )

  args = parser.parse_args()

  try:
    with open(args.input_policies, "r", encoding="utf-8") as f:
      simple_policies = json.load(f)

    device_schema = generate_device_policy_schema(args.manual_map)

    policy_blob = {}

    policy_user = simple_policies.get("policy_user")
    if not policy_user:
      raise ValueError("'policy_user' must be defined in the policy file.")
    policy_blob["policy_user"] = policy_user

    managed_users = simple_policies.get("managed_users", ["*"])
    policy_blob["managed_users"] = managed_users

    policy_blob["policies"] = []

    optional_params = [
        "allow_set_device_attributes",
        "current_key_index",
        "device_affiliation_ids",
        "directory_api_id",
        "initial_enrollment_state",
        "request_errors",
        "robot_api_auth_code",
        "user_affiliation_ids",
        "use_universal_signing_keys",
    ]
    for param in optional_params:
      if param in simple_policies:
        policy_blob[param] = simple_policies[param]

    if "user" in simple_policies:
      user_settings = chrome_settings_pb2.ChromeSettingsProto()
      apply_user_policies(simple_policies["user"], user_settings)
      encoded_policy = base64.b64encode(
          user_settings.SerializeToString()).decode("utf-8")
      policy_blob["policies"].append({
          "policy_type": "google/chromeos/user",
          "value": encoded_policy
      })

    if "device" in simple_policies:
      device_settings = chrome_device_policy_pb2.ChromeDeviceSettingsProto()
      apply_device_policies(simple_policies["device"], device_settings,
                            device_schema)
      encoded_policy = base64.b64encode(
          device_settings.SerializeToString()).decode("utf-8")
      policy_blob["policies"].append({
          "policy_type": "google/chromeos/device",
          "value": encoded_policy
      })

    with open(args.output_blob, "w", encoding="utf-8") as f:
      json.dump(policy_blob, f, indent=2)

    logging.info(f"Successfully wrote policy blob to {args.output_blob}")

  except (ValueError, IOError, json.JSONDecodeError) as e:
    logging.critical(f"Error: {e}")
    sys.exit(1)


if __name__ == "__main__":
  main()
