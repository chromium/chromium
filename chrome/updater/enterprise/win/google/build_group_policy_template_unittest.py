#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests ADM/ADMX/ADML template file generation with reference `gold` files.

  Args:
    test_gold_adm_file: path to the reference `gold` adm file.
    test_gold_admx_file: path to the reference `gold` admx file.
    test_gold_adml_file: path to the reference `gold` adml file.
    output_path: output path for generated files.

For example:
```
python3 chrome/updater/enterprise/win/google/
            build_group_policy_template_unittest.py
  --test_gold_adm_file
      chrome/updater/test/data/enterprise/win/google/test_gold.adm
  --test_gold_admx_file
      chrome/updater/test/data/enterprise/win/google/test_gold.admx
  --test_gold_adml_file
      chrome/updater/test/data/enterprise/win/google/test_gold.adml
  --output_path out/Default
```

"""

import argparse
import filecmp
import generate_group_policy_template_adm
import generate_group_policy_template_admx
import os
import sys


def BuildGroupPolicyTemplateAdmxTest(test_gold_adm_file, test_gold_admx_file,
                                     test_gold_adml_file, output_path, apps):
    if not os.path.exists(output_path):
        os.makedirs(output_path)
    target_adm = os.path.join(output_path, 'test_out.adm')
    target_admx = os.path.join(output_path, 'test_out.admx')
    target_adml = os.path.join(output_path, 'test_out.adml')

    generate_group_policy_template_adm.WriteGroupPolicyTemplate(
        target_adm, apps)
    adm_files_equal = filecmp.cmp(test_gold_adm_file,
                                  target_adm,
                                  shallow=False)
    if not adm_files_equal:
        print('FAIL: ADM files are not equal.')

    generate_group_policy_template_admx.WriteGroupPolicyTemplateAdmx(
        target_admx, apps)
    admx_files_equal = filecmp.cmp(test_gold_admx_file,
                                   target_admx,
                                   shallow=False)
    if not admx_files_equal:
        print('FAIL: ADMX files are not equal.')

    generate_group_policy_template_admx.WriteGroupPolicyTemplateAdml(
        target_adml, apps)
    adml_files_equal = filecmp.cmp(test_gold_adml_file,
                                   target_adml,
                                   shallow=False)
    if not adml_files_equal:
        print('FAIL: ADML files are not equal.')

    if adm_files_equal and admx_files_equal and adml_files_equal:
        print('SUCCESS. contents are equal')
        sys.exit(0)
    else:
        sys.exit(-1)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--test_gold_adm_file',
                        required=True,
                        help='path to the reference `gold` adm file')
    parser.add_argument('--test_gold_admx_file',
                        required=True,
                        help='path to the reference `gold` admx file')
    parser.add_argument('--test_gold_adml_file',
                        required=True,
                        help='path to the reference `gold` adml file')
    parser.add_argument('--output_path',
                        required=True,
                        help='output path for generated files')
    args = parser.parse_args()

    TEST_APPS = [
        ('Google Test Foo', '{D6B08267-B440-4c85-9F79-E195E80D9937}',
         ' Check http://www.google.com/test_foo/.', 'Disclaimer', True, True),
        (u'Google User Test Foo\u00a9\u00ae\u2122',
         '{104844D6-7DDA-460b-89F0-FBF8AFDD0A67}',
         ' Check http://www.google.com/user_test_foo/.', '', False, True),
    ]
    BuildGroupPolicyTemplateAdmxTest(args.test_gold_adm_file,
                                     args.test_gold_admx_file,
                                     args.test_gold_adml_file,
                                     args.output_path, TEST_APPS)


if __name__ == '__main__':
    main()
