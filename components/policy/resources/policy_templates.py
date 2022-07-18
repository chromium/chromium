#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys

POLICY_TEMPLATES_PATH =  os.path.join(
  os.path.dirname(__file__), 'policy_templates.json')

def GetPolicyTemplates():
  '''Returns an object containing the policy templates.'''

  with open(POLICY_TEMPLATES_PATH, 'r', encoding='utf-8') as f:
    return eval(f.read())


def main():
  '''Generates the a JSON file at `dest` with all the policy definitions.

    Args:
      dest: A path to the policy templates generated definitions.
  '''
  parser = argparse.ArgumentParser()
  parser.add_argument('--dest', dest='dest')
  args = parser.parse_args()

  with open(os.path.join(args.dest), 'w+', encoding='utf-8') as dest:
    json.dump(GetPolicyTemplates(), dest, indent=2, sort_keys=True)


if '__main__' == __name__:
  sys.exit(main())
