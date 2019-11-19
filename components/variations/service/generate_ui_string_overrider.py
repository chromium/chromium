#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import hashlib
import operator
import os
import re
import sys

SCRIPT_NAME = "generate_ui_string_overrider.py"


# Regular expression for parsing the #define macro format. Matches both the
# version of the macro with whitelist support and the one without. For example,
# Without generate whitelist flag:
#   #define IDS_FOO_MESSAGE 1234
# With generate whitelist flag:
#   #define IDS_FOO_MESSAGE (::ui::WhitelistedResource<1234>(), 1234)
RESOURCE_EXTRACT_REGEX = re.compile('^#define (\S*).* (\d+)\)?$', re.MULTILINE)

class Error(Exception):
  """Base error class for all exceptions in generated_resources_map."""


class HashCollisionError(Error):
  """Multiple resource names hash to the same value."""


Resource = collections.namedtuple("Resource", ['hash', 'name', 'index'])


def HashName(name):
  """Returns the hash id for a name.

  Args:
    name: The name to hash.

  Returns:
    An int that is at most 32 bits.
  """
  md5hash = hashlib.md5()
  md5hash.update(name)
  return int(md5hash.hexdigest()[:8], 16)


def _GetNameIndexPairsIter(string_to_scan):
  """Gets an iterator of the resource name and index pairs of the given string.

  Scans the input string for lines of the form "#define NAME INDEX" and returns
  an iterator over all matching (NAME, INDEX) pairs.

  Args:
    string_to_scan: The input string to scan.

  Yields:
    A tuple of name and index.
  """
  for match in RESOURCE_EXTRACT_REGEX.finditer(string_to_scan):
    yield match.group(1, 2)


def _GetResourceListFromString(resources_content):
  """Produces a list of |Resource| objects from a string.

  The input string contains lines of the form "#define NAME INDEX". The returned
  list is sorted primarily by hash, then name, and then index.

  Args:
    resources_content: The input string to process, contains lines of the form
        "#define NAME INDEX".

  Returns:
    A sorted list of |Resource| objects.
  """
  resources = [Resource(HashName(name), name, index) for name, index in
               _GetNameIndexPairsIter(resources_content)]

  # Deduplicate resources. Some name-index pairs appear in both chromium_ and
  # google_chrome_ header files. Unless deduplicated here, collisions will be
  # raised in _CheckForHashCollisions.
  resources = list(set(resources))

  # The default |Resource| order makes |resources| sorted by the hash, then
  # name, then index.
  resources.sort()

  return resources


def _CheckForHashCollisions(sorted_resource_list):
  """Checks a sorted list of |Resource| objects for hash collisions.

  Args:
    sorted_resource_list: A sorted list of |Resource| objects.

  Returns:
    A set of all |Resource| objects with collisions.
  """
  collisions = set()
  for i in xrange(len(sorted_resource_list) - 1):
    resource = sorted_resource_list[i]
    next_resource = sorted_resource_list[i+1]
    if resource.hash == next_resource.hash:
      collisions.add(resource)
      collisions.add(next_resource)

  return collisions


def _GenDataArray(
    resources, entry_pattern, array_name, array_type, data_getter):
  """Generates a C++ statement defining a literal array containing the hashes.

  Args:
    resources: A sorted list of |Resource| objects.
    entry_pattern: A pattern to be used to generate each entry in the array. The
        pattern is expected to have a place for data and one for a comment, in
        that order.
    array_name: The name of the array being generated.
    array_type: The type of the array being generated.
    data_getter: A function that gets the array data from a |Resource| object.

  Returns:
    A string containing a C++ statement defining the an array.
  """
  lines = [entry_pattern % (data_getter(r), r.name) for r in resources]
  pattern = """const %(type)s %(name)s[] = {
%(content)s
};
"""
  return pattern % {'type': array_type,
                    'name': array_name,
                    'content': '\n'.join(lines)}


def _GenerateNamespacePrefixAndSuffix(namespace):
  """Generates the namespace prefix and suffix for |namespace|.

  Args:
    namespace: A string corresponding to the namespace name. May be empty.

  Returns:
    A tuple of strings corresponding to the namespace prefix and suffix for
    putting the code in the corresponding namespace in C++. If namespace is
    the empty string, both returned strings are empty too.
  """
  if not namespace:
    return "", ""
  return "namespace %s {\n\n" % namespace, "\n}  // namespace %s\n" % namespace


def _GenerateSourceFileContent(resources_content, namespace, header_filename):
  """Generates the .cc content from the given generated grit headers content.

  Args:
    resources_content: The input string to process, contains lines of the form
        "#define NAME INDEX".

    namespace: The namespace in which the generated code should be scoped. If
        not defined, then the code will be in the global namespace.

    header_filename: Path to the corresponding .h.

  Returns:
    .cc file content implementing the CreateUIStringOverrider() factory.
  """
  hashed_tuples = _GetResourceListFromString(resources_content)

  collisions = _CheckForHashCollisions(hashed_tuples)
  if collisions:
    error_message = "\n".join(
        ["hash: %i, name: %s" % (i.hash, i.name) for i in sorted(collisions)])
    error_message = ("\nThe following names had hash collisions "
                     "(sorted by the hash value):\n%s\n" %(error_message))
    raise HashCollisionError(error_message)

  hashes_array = _GenDataArray(
      hashed_tuples, "    %iU,  // %s", 'kResourceHashes', 'uint32_t',
      operator.attrgetter('hash'))
  indices_array = _GenDataArray(
      hashed_tuples, "    %s,  // %s", 'kResourceIndices', 'int',
      operator.attrgetter('index'))

  namespace_prefix, namespace_suffix = _GenerateNamespacePrefixAndSuffix(
      namespace)

  return (
      "// This file was generated by %(script_name)s. Do not edit.\n"
      "\n"
      "#include \"%(header_filename)s\"\n\n"
      "%(namespace_prefix)s"
      "namespace {\n\n"
      "const size_t kNumResources = %(num_resources)i;\n\n"
      "%(hashes_array)s"
      "\n"
      "%(indices_array)s"
      "\n"
      "}  // namespace\n"
      "\n"
      "variations::UIStringOverrider CreateUIStringOverrider() {\n"
      "  return variations::UIStringOverrider(\n"
      "      kResourceHashes, kResourceIndices, kNumResources);\n"
      "}\n"
      "%(namespace_suffix)s") % {
          'script_name': SCRIPT_NAME,
          'header_filename': header_filename,
          'namespace_prefix': namespace_prefix,
          'num_resources': len(hashed_tuples),
          'hashes_array': hashes_array,
          'indices_array': indices_array,
          'namespace_suffix': namespace_suffix,
      }


def _GenerateHeaderFileContent(namespace, header_filename):
  """Generates the .h for to the .cc generated by _GenerateSourceFileContent.

  Args:
    namespace: The namespace in which the generated code should be scoped. If
        not defined, then the code will be in the global namespace.

    header_filename: Path to the corresponding .h. Used to generate the include
        guards.

  Returns:
    .cc file content implementing the CreateUIStringOverrider() factory.
  """

  include_guard = re.sub('[^A-Z]', '_', header_filename.upper()) + '_'
  namespace_prefix, namespace_suffix = _GenerateNamespacePrefixAndSuffix(
      namespace)

  return (
      "// This file was generated by %(script_name)s. Do not edit.\n"
      "\n"
      "#ifndef %(include_guard)s\n"
      "#define %(include_guard)s\n"
      "\n"
      "#include \"components/variations/service/ui_string_overrider.h\"\n\n"
      "%(namespace_prefix)s"
      "// Returns an initialized UIStringOverrider.\n"
      "variations::UIStringOverrider CreateUIStringOverrider();\n"
      "%(namespace_suffix)s"
      "\n"
      "#endif  // %(include_guard)s\n"
      ) % {
          'script_name': SCRIPT_NAME,
          'include_guard': include_guard,
          'namespace_prefix': namespace_prefix,
          'namespace_suffix': namespace_suffix,
      }


def main():
  arg_parser = argparse.ArgumentParser(
      description="Generate UIStringOverrider factory from resources headers "
                  "generated by grit.")
  arg_parser.add_argument(
      "--output_dir", "-o", required=True,
      help="Base directory to for generated files.")
  arg_parser.add_argument(
      "--source_filename", "-S", required=True,
      help="File name of the generated source file.")
  arg_parser.add_argument(
      "--header_filename", "-H", required=True,
      help="File name of the generated header file.")
  arg_parser.add_argument(
      "--namespace", "-N", default="",
      help="Namespace of the generated factory function (code will be in "
           "the global namespace if this is omitted).")
  arg_parser.add_argument(
      "--test_support", "-t", action="store_true", default=False,
      help="Make internal variables accessible for testing.")
  arg_parser.add_argument(
      "inputs", metavar="FILENAME", nargs="+",
      help="Path to resources header file generated by grit.")
  arguments = arg_parser.parse_args()

  generated_resources_h = ""
  for resources_file in arguments.inputs:
    with open(resources_file, "r") as resources:
      generated_resources_h += resources.read()

  if len(generated_resources_h) == 0:
    raise Error("No content loaded for %s." % (resources_file))

  source_file_content = _GenerateSourceFileContent(
      generated_resources_h, arguments.namespace, arguments.header_filename)
  header_file_content = _GenerateHeaderFileContent(
      arguments.namespace, arguments.header_filename)

  with open(os.path.join(
      arguments.output_dir, arguments.source_filename), "w") as generated_file:
    generated_file.write(source_file_content)
  with open(os.path.join(
      arguments.output_dir, arguments.header_filename), "w") as generated_file:
    generated_file.write(header_file_content)


if __name__ == '__main__':
  sys.exit(main())
