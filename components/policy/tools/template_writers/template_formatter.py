#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Takes translated policy_template.json files as input, applies template
writers and emits various template and doc files (admx, html, json etc.).
'''

import codecs
import collections
import json
import optparse
import os
import re
import sys

import writer_configuration
import policy_template_generator

from writers import adm_writer, adml_writer, admx_writer, \
                    chromeos_admx_writer, chromeos_adml_writer, \
                    google_admx_writer, google_adml_writer, \
                    android_policy_writer, reg_writer, doc_writer, \
                    doc_atomic_groups_writer , json_writer, plist_writer, \
                    plist_strings_writer


def MacLanguageMap(lang):
  '''Handles slightly different path naming convention for Macs:
  - 'en-US' -> 'en'
  - '-' -> '_'

  Args:
    lang: Language, e.g. 'en-US'.
  '''
  return 'en' if lang == 'en-US' else lang.replace('-', '_')


'''Template writer descriptors.

Members:
  type: Writer type, e.g. 'admx'
  is_per_language: Whether one file per language should be emitted.
  encoding: Encoding of the output file.
  language_map: Optional language mapping for file paths.
  force_windows_line_ending: Forces output file to use Windows line ending.
'''
WriterDesc = collections.namedtuple('WriterDesc', [
    'type', 'is_per_language', 'encoding', 'language_map',
    'force_windows_line_ending'
])

_WRITER_DESCS = [
    WriterDesc('adm', True, 'utf-16', None, True),
    WriterDesc('adml', True, 'utf-16', None, True),
    WriterDesc('admx', False, 'utf-16', None, True),
    WriterDesc('google_adml', True, 'utf-8', None, True),
    WriterDesc('google_admx', False, 'utf-8', None, True),
    WriterDesc('chromeos_adml', True, 'utf-8', None, True),
    WriterDesc('chromeos_admx', False, 'utf-8', None, True),
    WriterDesc('android_policy', False, 'utf-8', None, False),
    WriterDesc('reg', False, 'utf-16', None, False),
    WriterDesc('doc', True, 'utf-8', None, False),
    WriterDesc('doc_atomic_groups', True, 'utf-8', None, False),
    WriterDesc('json', False, 'utf-8', None, False),
    WriterDesc('plist', False, 'utf-8', None, False),
    WriterDesc('plist_strings', True, 'utf-8', MacLanguageMap, False)
]

# Template writers that are not per-language use policy_templates.json from
# this language.
_DEFAULT_LANGUAGE = 'en-US'


def GetWriter(writer_type, config):
  '''Returns the template writer for the given writer type.

  Args:
    writer_type: Writer type, e.g. 'admx'.
    config: Writer configuration, see writer_configuration.py.
  '''
  return eval(writer_type + '_writer.GetWriter(config)')


def _GetWriterConfiguration(grit_defines):
  '''Returns the writer configuration based on grit defines.

  Args:
    grit_defines: Array of grit defines, see grit_rule.gni.
  '''
  # Build a dictionary from grit defines, which can be plain DEFs or KEY=VALUEs.
  grit_defines_dict = {}
  for define in grit_defines:
    parts = define.split('=', 1)
    grit_defines_dict[parts[0]] = parts[1] if len(parts) > 1 else 1
  return writer_configuration.GetConfigurationForBuild(grit_defines_dict)


def _ParseVersionFile(version_path):
  '''Parse version file, return major version if it exists.

  Args:
    version_path: The path of Chrome VERSION file containing the major version
                  number.
  '''

  with open(version_path) as fp:
    for line in fp:
      key, _, major_version = line.partition('=')
      if key.strip() == 'MAJOR':
        return int(major_version.strip())
  return None


def _JsonToUtf8Encoding(data, ignore_dicts=False):
  if isinstance(data, unicode):
    return data.encode('utf-8')
  elif isinstance(data, list):
    return [_JsonToUtf8Encoding(item, False) for item in data]
  elif isinstance(data, dict):
    return {
        _JsonToUtf8Encoding(key): _JsonToUtf8Encoding(value)
        for key, value in data.iteritems()
    }
  return data


def main(argv):
  '''Main policy template conversion script.
  Usage: template_formatter
           --translations <translations_path>
           --languages <language_list>
           [--adm <adm_path>]
           ...
           [--android_policy <android_policy_path>]
           -D <grit_define>
           -E <grit_env_variable>
           -t <grit_target_platform>

  Args:
    translations: Absolute path of the translated policy_template.json
        files. Must contain a ${lang} placeholder for the language.
    languages: Comma-separated list of languages. Trailing commas are fine, e.g.
        'en,de,'
    adm, adml, google_adml, doc, plist_string: Absolute path of the
        corresponding file types. Must contain a ${lang} placeholder.
    admx, google_admx, android_policy, reg, json, plist: Absolute path of the
        corresponding file types. Must NOT contain a ${lang} placeholder. There
        is only one output file, not one per language.
    D: List of grit defines, used to assemble writer configuration.
    E, t: Grit environment variables and target platform. Unused, but
        grit_rule.gni adds them, so OptionParser must handle them.
  '''
  parser = optparse.OptionParser()
  parser.add_option('--translations', dest='translations')
  parser.add_option('--languages', dest='languages')
  parser.add_option('--version_path', dest='version_path')
  parser.add_option('--adm', action='append', dest='adm')
  parser.add_option('--adml', action='append', dest='adml')
  parser.add_option('--admx', action='append', dest='admx')
  parser.add_option('--chromeos_adml', action='append', dest='chromeos_adml')
  parser.add_option('--chromeos_admx', action='append', dest='chromeos_admx')
  parser.add_option('--google_adml', action='append', dest='google_adml')
  parser.add_option('--google_admx', action='append', dest='google_admx')
  parser.add_option('--reg', action='append', dest='reg')
  parser.add_option('--doc', action='append', dest='doc')
  parser.add_option(
      '--doc_atomic_groups', action='append', dest='doc_atomic_groups')
  parser.add_option(
      '--local',
      action='store_true',
      help='If set, the documentation will be built so \
            that links work locally in the generated path.')
  parser.add_option('--json', action='append', dest='json')
  parser.add_option('--plist', action='append', dest='plist')
  parser.add_option('--plist_strings', action='append', dest='plist_strings')
  parser.add_option('--android_policy', action='append', dest='android_policy')
  parser.add_option('-D', action='append', dest='grit_defines')
  parser.add_option('-E', action='append', dest='grit_build_env')
  parser.add_option('-t', action='append', dest='grit_target')
  options, args = parser.parse_args(argv[1:])

  _LANG_PLACEHOLDER = "${lang}"
  assert _LANG_PLACEHOLDER in options.translations

  languages = filter(bool, options.languages.split(','))
  assert _DEFAULT_LANGUAGE in languages

  config = _GetWriterConfiguration(options.grit_defines)
  config['major_version'] = _ParseVersionFile(options.version_path)
  config['local'] = options.local

  # For each language, load policy data once and run all writers on it.
  for lang in languages:
    # Load the policy data.
    policy_templates_json_path = options.translations.replace(
        _LANG_PLACEHOLDER, lang)
    # Loads the localized policy json file which must be a valid json file
    # encoded in utf-8.
    with codecs.open(policy_templates_json_path, 'r', 'utf-8') as policy_file:
      policy_data = json.loads(
          policy_file.read(), object_hook=_JsonToUtf8Encoding)

    # Preprocess the policy data.
    policy_generator = policy_template_generator.PolicyTemplateGenerator(
        config, policy_data)

    for writer_desc in _WRITER_DESCS:
      # For writer types that are not per language (e.g. admx), only do it once.
      if (not writer_desc.is_per_language and lang != _DEFAULT_LANGUAGE):
        continue

      # Was the current writer type passed as argument, e.g. --admx <path>?
      # Note that all paths are arrays and we loop over all of them.
      output_paths = getattr(options, writer_desc.type, '')
      if (not output_paths):
        continue
      for output_path in output_paths:
        # Substitute language placeholder in output file.
        if (writer_desc.is_per_language):
          assert _LANG_PLACEHOLDER in output_path
          mapped_lang = writer_desc.language_map(
              lang) if writer_desc.language_map else lang
          output_path = output_path.replace(_LANG_PLACEHOLDER, mapped_lang)
        else:
          assert _LANG_PLACEHOLDER not in output_path

        # Run the template writer on th policy data.
        writer = GetWriter(writer_desc.type, config)
        output_data = policy_generator.GetTemplateText(writer)
        # Make sure the file uses Windows line endings if needed.  This is
        # important here because codecs.open() opens files in binary more and
        # will not do line ending conversion.
        if writer_desc.force_windows_line_ending:
          output_data = re.sub(r'([^\r])\n', r'\1\r\n', output_data)

        # Make output directory if it doesn't exist yet.
        output_dir = os.path.split(output_path)[0]
        if not os.path.exists(output_dir):
          os.makedirs(output_dir)

        # Write output file.
        with codecs.open(output_path, 'w', writer_desc.encoding) as output_file:
          output_file.write(output_data)


if '__main__' == __name__:
  sys.exit(main(sys.argv))
