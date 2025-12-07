# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys


def process_proto_file(input_path, output_path, local_import_basenames):
  """Reads an input .proto file and writes a processed version.

  Transformations:
  1. Removes 'option optimize_for = LITE_RUNTIME;'.
  2. Uncomments blocks between '// copybara:uncomment_begin/end'.
  3. Rewrites any import statement that points to another file being processed
     in the same batch to use the "_full_proto.proto" suffix.
  """
  # Regex to find and capture the path in an import statement.
  import_regex = re.compile(r'import\s+"([^"]+\.proto)"')

  # Ensure the output directory exists.
  os.makedirs(os.path.dirname(output_path), exist_ok=True)

  with open(input_path, "r") as f_in, open(output_path, "w") as f_out:
    in_annotation_block = False
    for line in f_in:
      # Rule 1: Remove LITE_RUNTIME option.
      if "option optimize_for = LITE_RUNTIME;" in line:
        continue

      # Rule 2: Handle annotation blocks.
      if "copybara:uncomment_begin" in line:
        in_annotation_block = True
        continue
      if "copybara:uncomment_end" in line:
        in_annotation_block = False
        continue

      # If we are in an annotation block, uncomment the line before
      # further processing.
      if in_annotation_block:
        line = re.sub(r"^(\s*)//\s?(.*)$", r"\1\2", line)

      # Rule 3: Rewrite local imports to use the "_full_proto" suffix.
      match = import_regex.search(line)
      # Check if this line is an import and if the file it imports is one
      # of the other files we are processing in this batch.
      if match and os.path.basename(match.group(1)) in local_import_basenames:
        original_import_path = match.group(1)
        new_import_path = original_import_path.replace(
            ".proto", "_full_proto.proto"
        )
        line = f'import "{new_import_path}";\n'

      f_out.write(line)


def main():
  """Main function to handle command-line arguments.

  It now correctly parses the arguments provided by the BUILD.gn action.
  """
  if len(sys.argv) < 3:
    print(
        "Usage: python3 prepare_full_proto.py <output_dir>"
        " <full_input_path1> ..."
    )
    sys.exit(1)

  output_dir = sys.argv[1]

  input_paths = sys.argv[2:]

  local_import_basenames = {os.path.basename(p) for p in input_paths}

  for input_path in input_paths:
    output_basename = os.path.basename(input_path).replace(
        ".proto", "_full_proto.proto"
    )
    output_path = os.path.join(output_dir, output_basename)
    process_proto_file(input_path, output_path, local_import_basenames)


if __name__ == "__main__":
  main()
