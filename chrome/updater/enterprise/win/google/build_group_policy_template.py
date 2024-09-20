#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Builds Group Policy ADM/ADMX/ADML template files.

  Args:
    updater_adm_file: path to the output ADM file.
    updater_admx_file: path to the output ADMX file.
    updater_adml_file: path to the output ADML file.

For example:
```
python3 chrome/updater/enterprise/win/google/build_group_policy_template.py
  --updater_adm_file UpdaterAdm/updater.adm
  --updater_admx_file UpdaterAdmx/updater.admx
  --updater_adml_file UpdaterAdmx/en-US/updater.adml
```

"""

import argparse
import generate_group_policy_template_adm
import generate_group_policy_template_admx
import os
import os.path
import public_apps


def BuildGroupPolicyTemplateAdmx(target_adm, target_admx, target_adml, apps):
    for filename in [target_adm, target_admx, target_adml]:
        dirname = os.path.dirname(filename)
        if not os.path.exists(dirname):
            os.makedirs(dirname)
    generate_group_policy_template_adm.WriteGroupPolicyTemplate(
        target_adm, apps)
    generate_group_policy_template_admx.WriteGroupPolicyTemplateAdmx(
        target_admx, apps)
    generate_group_policy_template_admx.WriteGroupPolicyTemplateAdml(
        target_adml, apps)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--updater_adm_file',
                        required=True,
                        help='path to the output updater adm file')
    parser.add_argument('--updater_admx_file',
                        required=True,
                        help='path to the output updater admx file')
    parser.add_argument('--updater_adml_file',
                        required=True,
                        help='path to the output updater adml file')
    args = parser.parse_args()

    # `public_apps.EXTERNAL_APPS` contains a list of tuples containing
    # information about each app. See `generate_group_policy_template_adm` and
    # `generate_group_policy_template_admx` for details.
    BuildGroupPolicyTemplateAdmx(args.updater_adm_file, args.updater_admx_file,
                                 args.updater_adml_file,
                                 public_apps.EXTERNAL_APPS)


if __name__ == '__main__':
    main()
