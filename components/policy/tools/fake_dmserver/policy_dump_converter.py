#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This tool takes a JSON file exported from the chrome://policy page and
converts it into the simplified policies.json format used by the
fake_dmserver tools. This is useful for replicating an existing policy
configuration for local testing.
"""

import argparse
import json
import logging
import sys

logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')


def main():
  """Main script execution."""
  parser = argparse.ArgumentParser(
      description="Converts a chrome://policy JSON dump to the simple "
      "policies.json format.",
      epilog="""For detailed usage instructions, please refer to the
README.md in this directory.""",
      formatter_class=argparse.RawTextHelpFormatter)

  parser.add_argument(
      "--input-dump",
      required=True,
      help="Path to the JSON file exported from chrome://policy.")
  parser.add_argument("--output-policies",
                      required=True,
                      help="Path to write the simplified policies.json file.")
  parser.add_argument(
      "--policy-user",
      required=True,
      help="The managed user email to include in the output file.")

  args = parser.parse_args()

  try:
    with open(args.input_dump, "r", encoding="utf-8") as f:
      policy_dump = json.load(f)
  except (IOError, json.JSONDecodeError) as e:
    logging.critical(
        f"Failed to read or parse input file '{args.input_dump}': {e}")
    sys.exit(1)

  simple_policies = {
      "policy_user": args.policy_user,
      "managed_users": ["*"],
      "user": {},
      "device": {}
  }

  # The JSON dump can have different top-level keys. We need to find the
  # actual list of policies.
  policy_list = policy_dump.get('policyValues', {}).get('chrome',
                                                        {}).get('policies')

  if not policy_list:
    logging.critical("Could not find a list of policies in the input JSON. "
                     "Expected a dictionary containing a "
                     "'policyValues.chrome.policies' path.")
    sys.exit(1)

  # Convert the dictionary of policies to a list of policies.
  # The original script expected a list of policies, so we convert it here.
  policy_list_converted = []
  for name, details in policy_list.items():
    policy_list_converted.append({
        'name': name,
        'value': details.get('value'),
        'scope': details.get('scope')
    })
  policy_list = policy_list_converted

  for policy in policy_list:
    name = policy.get('name')
    value = policy.get('value')
    scope = policy.get('scope')

    if not name or not scope:
      logging.warning(f"Skipping entry with missing name or scope: {policy}")
      continue

    if value is None:
      logging.warning(f"Skipping policy '{name}' with a None value.")
      continue

    if scope == 'user':
      simple_policies['user'][name] = value
    elif scope == 'device':
      simple_policies['device'][name] = value
    else:
      logging.warning(f"Skipping policy '{name}' with unknown scope: '{scope}'")

  try:
    with open(args.output_policies, "w", encoding="utf-8") as f:
      json.dump(simple_policies, f, indent=2)
    logging.info(f"Successfully converted policies to {args.output_policies}")
  except IOError as e:
    logging.critical(f"Failed to write output file: {e}")
    sys.exit(1)


if __name__ == "__main__":
  main()
