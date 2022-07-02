#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys

POLICY_TEMPLATES_PATH = os.path.join('.', 'policy_templates.json')

def GetPolicyTemplates(path = POLICY_TEMPLATES_PATH):
  '''Returns an object containing the policy templates found at `path`.

    Args:
      path: A path to the policy templates definitions.
  '''
  with open(path, 'r', encoding='utf-8') as f:
    return eval(f.read())


def main():
  '''Converts the policy templates found at  `src` into a JSON file at `dest`.

    Args:
      src: A path to the policy templates definitions.
      dest: A path to the policy templates generated definitions.
  '''
  parser = argparse.ArgumentParser()
  parser.add_argument('--src', dest='source', default=POLICY_TEMPLATES_PATH)
  parser.add_argument('--dest', dest='dest')
  args = parser.parse_args()

  data = GetPolicyTemplates(os.path.join(args.source))

  with open(os.path.join(args.dest), 'w+', encoding='utf-8') as dest:
    json.dump(data, dest, indent=2, sort_keys=True)


if '__main__' == __name__:
  sys.exit(main())
