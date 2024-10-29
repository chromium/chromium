# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
import tempfile
from pathlib import Path
import os
import sys
import glob

PARENT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir))

sys.path.insert(0, PARENT_ROOT)
import license_type, license_utils, \
  create_android_metadata_license, mapper, constants


class LicenseParserTest(unittest.TestCase):
  def _write_readme_file(self, content: str):
    with open(self.temp_file.name, 'w') as file:
      file.write(content)

    return self.temp_file

  def setUp(self):
    self.temp_file = tempfile.NamedTemporaryFile(mode="w+", encoding="utf-8",
                                                 delete=False)

  def tearDown(self):
    os.remove(self.temp_file.name)

  def test_readme_chromium_parsing_correct(self):
    temp_file = self._write_readme_file("\n".join([
        "Name: some_name",
        "URL: some_site",
        "License: Apache 2.0",
        "License File: LICENSE"
    ]))
    metadata = license_utils.parse_chromium_readme_file(temp_file.name,
                                                        lambda x: x)
    self.assertEqual(metadata.get_licenses(), ["Apache 2.0"])
    self.assertEqual(metadata.get_name(), "some_name")
    self.assertEqual(metadata.get_url(), "some_site")
    self.assertEqual(metadata.get_license_file_path(), "LICENSE")

  def test_readme_chromium_parsing_post_process(self):
    temp_file = self._write_readme_file("\n".join([
        "Name: some_name",
        "URL: some_site",
        "License: Apache 2.0",
        "License File: LICENSE"
    ]))

    metadata = license_utils.parse_chromium_readme_file(temp_file.name,
                                                        constants.create_license_post_processing(
                                                            mapper.Mapper(
                                                                "License",
                                                                ["Apache 2.0"],
                                                                ["MPL 1.1"])))
    self.assertEqual(metadata.get_licenses(), ["MPL 1.1"])
    self.assertEqual(metadata.get_name(), "some_name")
    self.assertEqual(metadata.get_url(), "some_site")
    self.assertEqual(metadata.get_license_file_path(), "LICENSE")

  def test_readme_chromium_parsing_post_process_expected_check_fails(self):
    temp_file = self._write_readme_file("\n".join([
        "Name: some_name",
        "URL: some_site",
        "License: Apache 2.0",
        "License File: LICENSE"
    ]))

    self.assertRaisesRegex(Exception,
                           "Failed to post-process",
                           lambda: license_utils.parse_chromium_readme_file(
                               temp_file.name,
                               constants.create_license_post_processing(
                                   mapper.Mapper("License", ["Apache 2.1"],
                                                 ["MPL 1.1"]))))

  def test_readme_chromium_parsing_post_process_expected_check_fails_should_raise(
      self):
    temp_file = self._write_readme_file("\n".join([
        "Name: some_name",
        "URL: some_site",
        "License: Apache 2.0",
        "License File: LICENSE"
    ]))

    self.assertRaisesRegex(Exception,
                           "Failed to post-process",
                           lambda: license_utils.parse_chromium_readme_file(
                               temp_file.name,
                               constants.create_license_post_processing(
                                   mapper.Mapper("License", None,
                                                 ["MPL 1.1"]))))

  def test_readme_chromium_parsing_post_process_expected_check_with_no_key_fails_should_raise(
      self):
    temp_file = self._write_readme_file("\n".join([
        "Name: some_name",
        "URL: some_site",
        "License File: LICENSE"
    ]))

    self.assertRaisesRegex(Exception,
                           "Failed to post-process",
                           lambda: license_utils.parse_chromium_readme_file(
                               temp_file.name,
                               constants.create_license_post_processing(
                                   mapper.Mapper("License", "Apache 2.0",
                                                 ["MPL 1.1"]))))

  def test_readme_chromium_parsing_unidentified_license(self):
    temp_file = self._write_readme_file("\n".join([
        "Name: some_name",
        "URL: some_site",
        "License: FOO_BAR",
        "License File: LICENSE"
    ]))
    self.assertRaisesRegex(license_utils.InvalidMetadata,
                           "contains unidentified license \"FOO_BAR\"",
                           lambda: license_utils.parse_chromium_readme_file(
                               temp_file.name,
                               lambda x: x))

  def test_readme_chromium_parsing_no_license_file(self):
    temp_file = self._write_readme_file("\n".join([
        "Name: some_name",
        "URL: some_site",
        "License: Apache 2.0",
    ]))
    self.assertRaisesRegex(license_utils.InvalidMetadata,
                           "License file path not declared in",
                           lambda: license_utils.parse_chromium_readme_file(
                               temp_file.name,
                               lambda x: x))

  def test_readme_chromium_parsing_malformed(self):
    temp_file = self._write_readme_file("")

    self.assertRaisesRegex(Exception, "Failed to parse any valid metadata",
                           lambda: license_utils.parse_chromium_readme_file(
                               temp_file.name,
                               lambda x: x))

  def test_most_restrictive_license_comparison(self):
    self.assertEqual(
        license_utils.get_most_restrictive_type(["Apache 2.0", "MPL 1.1"]),
        license_type.LicenseType.RECIPROCAL)

  def test_generate_license_for_rust_crate(self):
    with tempfile.TemporaryDirectory() as temp_directory:
      # Rust directories in Chromium have a special structure that looks like
      # //third_party/rust
      #                   /my_crate/
      #                   /crates/my_crate/src
      # README.chromium and BUILD.gn lives in //third_party/rust/my_crate while
      # the actual crate lives in //third_party/rust/crates/my_crate/.
      #
      # This test verifies that we generate the licensing files in the directory
      # where the source lives and not where the README.chromium lives.
      #
      # See https://source.chromium.org/chromium/chromium/src/+/main:third_party/rust/
      readme_dir = Path(
          os.path.join(temp_directory, "third_party/rust/my_crate"))
      readme_dir.mkdir(parents=True)
      crate_path = Path(
          os.path.join(temp_directory, "third_party/rust/crates/my_crate/src"))
      crate_path.mkdir(parents=True)
      self._write_empty_file(os.path.join(crate_path, "COPYING"))
      with open(os.path.join(readme_dir, "README.chromium"),
                "w", encoding="utf-8") as readme:
        readme.write("\n".join([
            "Name: some_name",
            "URL: some_site",
            "License: Apache 2.0",
            "License File: //third_party/rust/crates/my_crate/src/COPYING"
        ]))
      # COPYING file must exist as we check for symlink to make sure that
      # they still exist.
      create_android_metadata_license.update_license(temp_directory, {})
      self.assertTrue(os.path.exists(os.path.join(crate_path, "METADATA")))
      self.assertTrue(os.path.islink(os.path.join(crate_path, "LICENSE")))
      self.assertTrue(os.path.exists(
          os.path.join(crate_path, "MODULE_LICENSE_APACHE_2_0")))
      metadata_content = Path(
          os.path.join(crate_path, "METADATA")).read_text()
      self.assertRegex(
          metadata_content,
          "name: \"some_name\"")
      self.assertRegex(
          metadata_content,
          "license_type: NOTICE")
      # Verify that the symlink is relative and not absolute path.
      self.assertRegex(os.readlink(os.path.join(crate_path, "LICENSE")),
                       "^[^\/].*",
                       "Symlink path must be relative.")

  def test_generate_license_for_temp_dir(self):
    with tempfile.TemporaryDirectory() as temp_directory:
      with open(os.path.join(temp_directory, "README.chromium"),
                "w", encoding="utf-8") as readme:
        readme.write("\n".join([
            "Name: some_name",
            "URL: some_site",
            "License: Apache 2.0",
            "License File: COPYING"
        ]))
      # COPYING file must exist as we check for symlink to make sure that
      # they still exist.
      self._write_empty_file(os.path.join(temp_directory, "COPYING"))
      create_android_metadata_license.update_license(temp_directory, {})
      self.assertTrue(
          os.path.exists(os.path.join(temp_directory, "METADATA")))
      self.assertTrue(os.path.islink(os.path.join(temp_directory, "LICENSE")))
      self.assertTrue(os.path.exists(
          os.path.join(temp_directory, "MODULE_LICENSE_APACHE_2_0")))
      metadata_content = Path(
          os.path.join(temp_directory, "METADATA")).read_text()
      self.assertRegex(
          metadata_content,
          "name: \"some_name\"")
      self.assertRegex(
          metadata_content,
          "license_type: NOTICE")

  def test_verify_only_mode_missing_metadata(self):
    with tempfile.TemporaryDirectory() as temp_directory:
      with open(os.path.join(temp_directory, "README.chromium"),
                "w", encoding="utf-8") as readme:
        readme.write("\n".join([
            "Name: some_name",
            "URL: some_site",
            "License: Apache 2.0",
            "License File: COPYING"
        ]))
      # COPYING file must exist as we check for symlink to make sure that
      # they still exist.
      self._write_empty_file(os.path.join(temp_directory, "COPYING"))
      create_android_metadata_license.update_license(temp_directory, {})
      self.assertTrue(
          os.path.exists(os.path.join(temp_directory, "METADATA")))
      os.remove(os.path.join(temp_directory, "METADATA"))
      self.assertRaisesRegex(Exception, "Failed to find metadata",
                             lambda: create_android_metadata_license.update_license(
                                 temp_directory, {},
                                 verify_only=True))

  def _write_empty_file(self, path: str):
    Path(path).touch()

  def test_verify_only_mode_content_mismatch(self):
    with tempfile.TemporaryDirectory() as temp_directory:
      with open(os.path.join(temp_directory, "README.chromium"),
                "w", encoding="utf-8") as readme:
        readme.write("\n".join([
            "Name: some_name",
            "URL: some_site",
            "License: Apache 2.0",
            "License File: COPYING"
        ]))
      # COPYING file must exist as we check for symlink to make sure that
      # they still exist.
      self._write_empty_file(os.path.join(temp_directory, "COPYING"))
      create_android_metadata_license.update_license(temp_directory, {})
      metadata_path = os.path.join(temp_directory, "METADATA")
      self.assertTrue(os.path.exists(metadata_path))
      # The METADATA header must exist otherwise it will be considered a
      # manually written file.
      Path(metadata_path).write_text("\n".join(
          [create_android_metadata_license.METADATA_HEADER,
           "some_random_text"]), encoding="utf-8")
      self.assertRaisesRegex(Exception,
                             "Please re-run create_android_metadata_license.py",
                             lambda: create_android_metadata_license.update_license(
                                 temp_directory, {},
                                 verify_only=True))

  def test_verify_only_mode_missing_license(self):
    with tempfile.TemporaryDirectory() as temp_directory:
      with open(os.path.join(temp_directory, "README.chromium"),
                "w", encoding="utf-8") as readme:
        readme.write("\n".join([
            "Name: some_name",
            "URL: some_site",
            "License: Apache 2.0",
            "License File: COPYING"
        ]))
      create_android_metadata_license.update_license(temp_directory, {})
      self.assertTrue(os.path.islink(os.path.join(temp_directory, "LICENSE")))
      os.remove(os.path.join(temp_directory, "LICENSE"))
      self.assertRaisesRegex(Exception, "License symlink does not exist",
                             lambda: create_android_metadata_license.update_license(
                                 temp_directory, {},
                                 verify_only=True))

  def test_verify_only_mode_bad_symlink(self):
    with tempfile.TemporaryDirectory() as temp_directory:
      with open(os.path.join(temp_directory, "README.chromium"),
                "w", encoding="utf-8") as readme:
        readme.write("\n".join([
            "Name: some_name",
            "URL: some_site",
            "License: Apache 2.0",
            "License File: COPYING"
        ]))
      create_android_metadata_license.update_license(temp_directory, {})
      self.assertTrue(os.path.islink(os.path.join(temp_directory, "LICENSE")))
      self.assertRaisesRegex(Exception, "License symlink does not exist",
                             lambda: create_android_metadata_license.update_license(
                                 temp_directory, {},
                                 verify_only=True))

  def test_verify_only_mode_missing_module(self):
    with tempfile.TemporaryDirectory() as temp_directory:
      with open(os.path.join(temp_directory, "README.chromium"),
                "w", encoding="utf-8") as readme:
        readme.write("\n".join([
            "Name: some_name",
            "URL: some_site",
            "License: Apache 2.0",
            "License File: COPYING"
        ]))
      # COPYING file must exist as we check for symlink to make sure that
      # they still exist.
      self._write_empty_file(os.path.join(temp_directory, "COPYING"))
      create_android_metadata_license.update_license(temp_directory, {})
      self.assertTrue(os.path.exists(
          os.path.join(temp_directory, "MODULE_LICENSE_APACHE_2_0")))
      os.remove(os.path.join(temp_directory, "MODULE_LICENSE_APACHE_2_0"))
      self.assertRaisesRegex(Exception, "Failed to find module file",
                             lambda: create_android_metadata_license.update_license(
                                 temp_directory, {},
                                 verify_only=True))

  @unittest.skip("b/372449684")
  def test_license_for_aosp(self):
    """This test verifies that external/cronet conforms to the licensing structure."""
    # When running inside the context of atest, the working directory contains
    # all the METADATA / README.chromium / LICENSE, etc.. files that we need.
    # hence why repo_path=os.getcwd()
    try:
      create_android_metadata_license.update_license(repo_path=os.getcwd(),
                                                     verify_only=True)
    except Exception:
      self.fail(
          "Please update the licensing by running create_android_metadata_license.py")


if __name__ == '__main__':
  unittest.main()