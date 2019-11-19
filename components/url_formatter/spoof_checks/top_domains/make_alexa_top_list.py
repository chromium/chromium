#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates alexa_domains.list from
   src/tools/perf/page_sets/alexa1-10000-urls.json.  By default, all the domains
   extracted from the input will be recorded in alexa_domains.list in the script
   directory except for duplicates and domains in ccTLDs known to disallow
   non-ASCII Latin letters (cn,jp,kr,tw).
   Optional command line arguments can be used to limit the output to top N
   domains and to specify an output file.
"""

import re
import sys
import os

script_dir = os.path.dirname(os.path.realpath(__file__))
alexa10k_path = os.path.join(script_dir, "..", "..", "..", "tools", "perf",
                              "page_sets", "alexa1-10000-urls.json")
max_num_domains = 10000 if len(sys.argv) < 2 else int(sys.argv[1])
alexa_out = os.path.join(script_dir, "alexa_domains.list") \
    if len(sys.argv) < 3 else os.path.join(script_dir, sys.argv[2])

domain_extractor = re.compile(r'^.*"https?://(?:www.)?([^/]*)/.*$')
excluded_tld = re.compile(r'.(cn|kr|jp|tw)$')
domains = set()
n_domains = 0

with open(alexa_out, 'w') as outfile, open(alexa10k_path, 'r') as infile:
  for line in infile:
    if line.startswith('#'):
      continue
    match = domain_extractor.match(line)
    if match and n_domains < max_num_domains:
      n_domains = n_domains + 1
      domain = match.group(1)
      labels = domain.split('.')
      if len(labels) > 3:
        domain = '.'.join(labels[-3:])
      if not excluded_tld.search(match.group(1)) and domain not in domains:
        domains.add(domain)
        outfile.write(domain + "\n")

  # Add some popular domains if they're missing.
  # TODO(jshin): Find a way to update the list. (crbug.com/722022)
  for domain in ["gmail.com", "hotmail.com", "360.cn", "ntd.tv", "onclkds.com",
                 "uber.com", "lyft.com", "ok.ru", "stripe.com"]:
    if domain not in domains:
      outfile.write(domain + "\n")
