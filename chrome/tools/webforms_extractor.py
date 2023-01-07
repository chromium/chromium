#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Extracts registration forms from the corresponding HTML files.

Used for extracting forms within HTML files. This script is used in
conjunction with the webforms_aggregator.py script, which aggregates web pages
with fillable forms (i.e registration forms).

The purpose of this script is to extract out all non-form elements that may be
causing parsing errors and timeout issues when running browser_tests.

This script extracts all forms from a HTML file.
If there are multiple forms per downloaded site, multiple files are created
for each form.

Used as a standalone script but assumes that it is run from the directory in
which it is checked into.

Usage: forms_extractor.py [options]

Options:
  -l LOG_LEVEL, --log_level=LOG_LEVEL,
    LOG_LEVEL: debug, info, warning or error [default: error]
  -j, --js  extracts javascript elements from web form.
  -h, --help  show this help message and exit
"""

import glob
import logging
from optparse import OptionParser
import os
import re
import sys


class FormsExtractor(object):
  """Extracts HTML files, leaving only registration forms from the HTML file."""
  _HTML_FILES_PATTERN = r'*.html'
  _HTML_FILE_PREFIX = r'grabber-'
  _FORM_FILE_PREFIX = r'grabber-stripped-'

  _REGISTRATION_PAGES_DIR = os.path.join(os.pardir, 'test', 'data', 'autofill',
                                         'heuristics', 'input')
  _EXTRACTED_FORMS_DIR = os.path.join(os.pardir, 'test', 'data', 'autofill',
                                      'heuristics', 'input')

  logger = logging.getLogger(__name__)
  log_handlers = {'StreamHandler': None}

  # This pattern is used for retrieving the form location comment located at the
  # top of each downloaded HTML file indicating where the form originated from.
  _RE_FORM_LOCATION_PATTERN = re.compile(
      ur"""
      <!--Form\s{1}Location:  # Starting of form location comment.
      .*?                     # Any characters (non-greedy).
      -->                     # Ending of the form comment.
      """, re.U | re.S | re.I | re.X)

  # This pattern is used for removing all script code.
  _RE_SCRIPT_PATTERN = re.compile(
      ur"""
      <script       # A new opening '<script' tag.
      \b            # The end of the word 'script'.
      .*?           # Any characters (non-greedy).
      >             # Ending of the (opening) tag: '>'.
      .*?           # Any characters (non-greedy) between the tags.
      </script\s*>  # The '</script>' closing tag.
      """, re.U | re.S | re.I | re.X)

  # This pattern is used for removing all href js code.
  _RE_HREF_JS_PATTERN = re.compile(
      ur"""
      \bhref             # The word href and its beginning.
      \s*=\s*            # The '=' with all whitespace before and after it.
      (?P<quote>[\'\"])  # A single or double quote which is captured.
      \s*javascript\s*:  # The word 'javascript:' with any whitespace possible.
      .*?                # Any characters (non-greedy) between the quotes.
      \1                 # The previously captured single or double quote.
      """, re.U | re.S | re.I | re.X)

  _RE_EVENT_EXPR = (
      ur"""
      \b                 # The beginning of a new word.
      on\w+?             # All words starting with 'on' (non-greedy)
                         # example: |onmouseover|.
      \s*=\s*            # The '=' with all whitespace before and after it.
      (?P<quote>[\'\"])  # A captured single or double quote.
      .*?                # Any characters (non-greedy) between the quotes.
      \1                 # The previously captured single or double quote.
      """)

  # This pattern is used for removing code with js events, such as |onload|.
  # By adding the leading |ur'<[^<>]*?'| and the trailing |'ur'[^<>]*?>'| the
  # pattern matches to strings such as '<tr class="nav"
  # onmouseover="mOvr1(this);" onmouseout="mOut1(this);">'
  _RE_TAG_WITH_EVENTS_PATTERN = re.compile(
      ur"""
      <        # Matches character '<'.
      [^<>]*?  # Matches any characters except '<' and '>' (non-greedy).""" +
      _RE_EVENT_EXPR +
      ur"""
      [^<>]*?  # Matches any characters except '<' and '>' (non-greedy).
      >        # Matches character '>'.
      """, re.U | re.S | re.I | re.X)

  # Adds whitespace chars at the end of the matched event. Also match trailing
  # whitespaces for JS events. Do not match leading whitespace.
  # For example: |< /form>| is invalid HTML and does not exist but |</form >| is
  # considered valid HTML.
  _RE_EVENT_PATTERN = re.compile(
      _RE_EVENT_EXPR + ur'\s*', re.U | re.S | re.I | re.X)

  # This pattern is used for finding form elements.
  _RE_FORM_PATTERN = re.compile(
      ur"""
      <form       # A new opening '<form' tag.
      \b          # The end of the word 'form'.
      .*?         # Any characters (non-greedy).
      >           # Ending of the (opening) tag: '>'.
      .*?         # Any characters (non-greedy) between the tags.
      </form\s*>  # The '</form>' closing tag.
      """, re.U | re.S | re.I | re.X)

  def __init__(self, input_dir=_REGISTRATION_PAGES_DIR,
               output_dir=_EXTRACTED_FORMS_DIR, logging_level=None):
    """Creates a FormsExtractor object.

    Args:
      input_dir: the directory of HTML files.
      output_dir: the directory where the registration form files will be
                  saved.
      logging_level: verbosity level, default is None.

    Raises:
      IOError exception if input directory doesn't exist.
    """
    if logging_level:
      if not self.log_handlers['StreamHandler']:
        console = logging.StreamHandler()
        console.setLevel(logging.DEBUG)
        self.log_handlers['StreamHandler'] = console
        self.logger.addHandler(console)
      self.logger.setLevel(logging_level)
    else:
      if self.log_handlers['StreamHandler']:
        self.logger.removeHandler(self.log_handlers['StreamHandler'])
        self.log_handlers['StreamHandler'] = None

    self._input_dir = input_dir
    self._output_dir = output_dir
    if not os.path.isdir(self._input_dir):
      error_msg = 'Directory "%s" doesn\'t exist.' % self._input_dir
      self.logger.error('Error: %s', error_msg)
      raise IOError(error_msg)
    if not os.path.isdir(output_dir):
      os.makedirs(output_dir)
    self._form_location_comment = ''

  def _SubstituteAllEvents(self, matchobj):
    """Remove all js events that are present as attributes within a tag.

    Args:
      matchobj: A regexp |re.MatchObject| containing text that has at least one
                event. Example: |<tr class="nav" onmouseover="mOvr1(this);"
                onmouseout="mOut1(this);">|.

    Returns:
      The text containing the tag with all the attributes except for the tags
      with events. Example: |<tr class="nav">|.
    """
    tag_with_all_attrs = matchobj.group(0)
    return self._RE_EVENT_PATTERN.sub('', tag_with_all_attrs)

  def Extract(self, strip_js_only):
    """Extracts and saves the extracted registration forms.

    Iterates through all the HTML files.

    Args:
      strip_js_only: If True, only Javascript is stripped from the HTML content.
                     Otherwise, all non-form elements are stripped.
    """
    pathname_pattern = os.path.join(self._input_dir, self._HTML_FILES_PATTERN)
    html_files = [f for f in glob.glob(pathname_pattern) if os.path.isfile(f)]
    for filename in html_files:
      self.logger.info('Stripping file "%s" ...', filename)
      with open(filename, 'U') as f:
        html_content = self._RE_TAG_WITH_EVENTS_PATTERN.sub(
            self._SubstituteAllEvents,
            self._RE_HREF_JS_PATTERN.sub(
                '', self._RE_SCRIPT_PATTERN.sub('', f.read())))

        form_filename = os.path.split(filename)[1]  # Path dropped.
        form_filename = form_filename.replace(self._HTML_FILE_PREFIX, '', 1)
        (form_filename, extension) = os.path.splitext(form_filename)
        form_filename = (self._FORM_FILE_PREFIX + form_filename +
                         '%s' + extension)
        form_filename = os.path.join(self._output_dir, form_filename)
        if strip_js_only:
          form_filename = form_filename % ''
          try:
            with open(form_filename, 'w') as f:
              f.write(html_content)
          except IOError as e:
            self.logger.error('Error: %s', e)
            continue
        else:  # Remove all non form elements.
          match = self._RE_FORM_LOCATION_PATTERN.search(html_content)
          if match:
            form_location_comment = match.group() + os.linesep
          else:
            form_location_comment = ''
          forms_iterator = self._RE_FORM_PATTERN.finditer(html_content)
          for form_number, form_match in enumerate(forms_iterator, start=1):
            form_content = form_match.group()
            numbered_form_filename = form_filename % form_number
            try:
              with open(numbered_form_filename, 'w') as f:
                f.write(form_location_comment)
                f.write(form_content)
            except IOError as e:
              self.logger.error('Error: %s', e)
              continue
          self.logger.info('\tFile "%s" extracted SUCCESSFULLY!', filename)


def main():
  parser = OptionParser()
  parser.add_option(
      '-l', '--log_level', metavar='LOG_LEVEL', default='error',
      help='LOG_LEVEL: debug, info, warning or error [default: %default]')
  parser.add_option(
      '-j', '--js', dest='js', action='store_true', default=False,
      help='Removes all javascript elements [default: %default]')

  (options, args) = parser.parse_args()
  options.log_level = options.log_level.upper()
  if options.log_level not in ['DEBUG', 'INFO', 'WARNING', 'ERROR']:
    print 'Wrong log_level argument.'
    parser.print_help()
    return 1

  options.log_level = getattr(logging, options.log_level)
  extractor = FormsExtractor(logging_level=options.log_level)
  extractor.Extract(options.js)
  return 0


if __name__ == '__main__':
  sys.exit(main())
