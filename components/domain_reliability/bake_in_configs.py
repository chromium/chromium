#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Takes the JSON files in components/domain_reliability/baked_in_configs and
encodes their contents as an array of C strings that gets compiled in to Chrome
and loaded at runtime."""

from __future__ import print_function

import ast
import json
import optparse
import os
import shlex
import sys


# A whitelist of domains that the script will accept when baking configs in to
# Chrome, to ensure incorrect ones are not added accidentally. Subdomains of
# whitelist entries are also allowed (e.g. maps.google.com, ssl.gstatic.com).
DOMAIN_WHITELIST = (
  '2mdn.net',
  'admob.biz',
  'admob.co.in',
  'admob.co.kr',
  'admob.co.nz',
  'admob.co.uk',
  'admob.co.za',
  'admob.com',
  'admob.com.br',
  'admob.com.es',
  'admob.com.fr',
  'admob.com.mx',
  'admob.com.pt',
  'admob.de',
  'admob.dk',
  'admob.es',
  'admob.fi',
  'admob.fr',
  'admob.gr',
  'admob.hk',
  'admob.ie',
  'admob.in',
  'admob.it',
  'admob.jp',
  'admob.kr',
  'admob.mobi',
  'admob.no',
  'admob.ph',
  'admob.pt',
  'admob.sg',
  'admob.tw',
  'admob.us',
  'admob.vn',
  'dartmotif.com',
  'doubleclick.com',
  'doubleclick.ne.jp',
  'doubleclick.net',
  'doubleclickusercontent.com',
  'g.co',
  'ggpht.com',
  'gmodules.com',
  'goo.gl',
  'google-analytics.com',
  'google-syndication.com',
  'google.ac',
  'google.ad',
  'google.ae',
  'google.af',
  'google.ag',
  'google.al',
  'google.am',
  'google.as',
  'google.at',
  'google.az',
  'google.ba',
  'google.be',
  'google.bf',
  'google.bg',
  'google.bi',
  'google.bj',
  'google.bs',
  'google.bt',
  'google.by',
  'google.ca',
  'google.cat',
  'google.cc',
  'google.cd',
  'google.cf',
  'google.cg',
  'google.ch',
  'google.ci',
  'google.cl',
  'google.cm',
  'google.cn',
  'google.co.ao',
  'google.co.bw',
  'google.co.ck',
  'google.co.cr',
  'google.co.hu',
  'google.co.id',
  'google.co.il',
  'google.co.im',
  'google.co.in',
  'google.co.je',
  'google.co.jp',
  'google.co.ke',
  'google.co.kr',
  'google.co.ls',
  'google.co.ma',
  'google.co.mz',
  'google.co.nz',
  'google.co.th',
  'google.co.tz',
  'google.co.ug',
  'google.co.uk',
  'google.co.uz',
  'google.co.ve',
  'google.co.vi',
  'google.co.za',
  'google.co.zm',
  'google.co.zw',
  'google.com',
  'google.com.af',
  'google.com.ag',
  'google.com.ai',
  'google.com.ar',
  'google.com.au',
  'google.com.bd',
  'google.com.bh',
  'google.com.bn',
  'google.com.bo',
  'google.com.br',
  'google.com.by',
  'google.com.bz',
  'google.com.cn',
  'google.com.co',
  'google.com.cu',
  'google.com.cy',
  'google.com.do',
  'google.com.ec',
  'google.com.eg',
  'google.com.et',
  'google.com.fj',
  'google.com.ge',
  'google.com.gh',
  'google.com.gi',
  'google.com.gr',
  'google.com.gt',
  'google.com.hk',
  'google.com.iq',
  'google.com.jm',
  'google.com.jo',
  'google.com.kh',
  'google.com.kw',
  'google.com.lb',
  'google.com.ly',
  'google.com.mm',
  'google.com.mt',
  'google.com.mx',
  'google.com.my',
  'google.com.na',
  'google.com.nf',
  'google.com.ng',
  'google.com.ni',
  'google.com.np',
  'google.com.nr',
  'google.com.om',
  'google.com.pa',
  'google.com.pe',
  'google.com.pg',
  'google.com.ph',
  'google.com.pk',
  'google.com.pl',
  'google.com.pr',
  'google.com.py',
  'google.com.qa',
  'google.com.ru',
  'google.com.sa',
  'google.com.sb',
  'google.com.sg',
  'google.com.sl',
  'google.com.sv',
  'google.com.tj',
  'google.com.tn',
  'google.com.tr',
  'google.com.tw',
  'google.com.ua',
  'google.com.uy',
  'google.com.vc',
  'google.com.ve',
  'google.com.vn',
  'google.cv',
  'google.cz',
  'google.de',
  'google.dj',
  'google.dk',
  'google.dm',
  'google.dz',
  'google.ee',
  'google.es',
  'google.fi',
  'google.fm',
  'google.fr',
  'google.ga',
  'google.ge',
  'google.gg',
  'google.gl',
  'google.gm',
  'google.gp',
  'google.gr',
  'google.gy',
  'google.hk',
  'google.hn',
  'google.hr',
  'google.ht',
  'google.hu',
  'google.ie',
  'google.im',
  'google.info',
  'google.iq',
  'google.ir',
  'google.is',
  'google.it',
  'google.it.ao',
  'google.je',
  'google.jo',
  'google.jobs',
  'google.jp',
  'google.kg',
  'google.ki',
  'google.kz',
  'google.la',
  'google.li',
  'google.lk',
  'google.lt',
  'google.lu',
  'google.lv',
  'google.md',
  'google.me',
  'google.mg',
  'google.mk',
  'google.ml',
  'google.mn',
  'google.ms',
  'google.mu',
  'google.mv',
  'google.mw',
  'google.ne',
  'google.ne.jp',
  'google.net',
  'google.ng',
  'google.nl',
  'google.no',
  'google.nr',
  'google.nu',
  'google.off.ai',
  'google.org',
  'google.pk',
  'google.pl',
  'google.pn',
  'google.ps',
  'google.pt',
  'google.ro',
  'google.rs',
  'google.ru',
  'google.rw',
  'google.sc',
  'google.se',
  'google.sh',
  'google.si',
  'google.sk',
  'google.sm',
  'google.sn',
  'google.so',
  'google.sr',
  'google.st',
  'google.td',
  'google.tg',
  'google.tk',
  'google.tl',
  'google.tm',
  'google.tn',
  'google.to',
  'google.tt',
  'google.us',
  'google.uz',
  'google.vg',
  'google.vu',
  'google.ws',
  'googleadservices.com',
  'googleadsserving.cn',
  'googlealumni.com',
  'googleapis.com',
  'googleapps.com',
  'googlecbs.com',
  'googlecommerce.com',
  'googledrive.com',
  'googleenterprise.com',
  'googlegoro.com',
  'googlehosted.com',
  'googlepayments.com',
  'googlesource.com',
  'googlesyndication.com',
  'googletagmanager.com',
  'googletagservices.com',
  'googleusercontent.com',
  'googlevideo.com',
  'gstatic.com',
  'gvt1.com',
  'gvt2.com',
  'gvt6.com',
  'withgoogle.com',
  'youtu.be',
  'youtube-3rd-party.com',
  'youtube-nocookie.com',
  'youtube.ae',
  'youtube.al',
  'youtube.am',
  'youtube.at',
  'youtube.az',
  'youtube.ba',
  'youtube.be',
  'youtube.bg',
  'youtube.bh',
  'youtube.bo',
  'youtube.ca',
  'youtube.cat',
  'youtube.ch',
  'youtube.cl',
  'youtube.co',
  'youtube.co.ae',
  'youtube.co.at',
  'youtube.co.hu',
  'youtube.co.id',
  'youtube.co.il',
  'youtube.co.in',
  'youtube.co.jp',
  'youtube.co.ke',
  'youtube.co.kr',
  'youtube.co.ma',
  'youtube.co.nz',
  'youtube.co.th',
  'youtube.co.ug',
  'youtube.co.uk',
  'youtube.co.ve',
  'youtube.co.za',
  'youtube.com',
  'youtube.com.ar',
  'youtube.com.au',
  'youtube.com.az',
  'youtube.com.bh',
  'youtube.com.bo',
  'youtube.com.br',
  'youtube.com.by',
  'youtube.com.co',
  'youtube.com.do',
  'youtube.com.ee',
  'youtube.com.eg',
  'youtube.com.es',
  'youtube.com.gh',
  'youtube.com.gr',
  'youtube.com.gt',
  'youtube.com.hk',
  'youtube.com.hr',
  'youtube.com.jm',
  'youtube.com.jo',
  'youtube.com.kw',
  'youtube.com.lb',
  'youtube.com.lv',
  'youtube.com.mk',
  'youtube.com.mt',
  'youtube.com.mx',
  'youtube.com.my',
  'youtube.com.ng',
  'youtube.com.om',
  'youtube.com.pe',
  'youtube.com.ph',
  'youtube.com.pk',
  'youtube.com.pt',
  'youtube.com.qa',
  'youtube.com.ro',
  'youtube.com.sa',
  'youtube.com.sg',
  'youtube.com.tn',
  'youtube.com.tr',
  'youtube.com.tw',
  'youtube.com.ua',
  'youtube.com.uy',
  'youtube.com.ve',
  'youtube.cz',
  'youtube.de',
  'youtube.dk',
  'youtube.ee',
  'youtube.es',
  'youtube.fi',
  'youtube.fr',
  'youtube.ge',
  'youtube.gr',
  'youtube.gt',
  'youtube.hk',
  'youtube.hr',
  'youtube.hu',
  'youtube.ie',
  'youtube.in',
  'youtube.is',
  'youtube.it',
  'youtube.jo',
  'youtube.jp',
  'youtube.kr',
  'youtube.lk',
  'youtube.lt',
  'youtube.lv',
  'youtube.ma',
  'youtube.md',
  'youtube.me',
  'youtube.mk',
  'youtube.mx',
  'youtube.my',
  'youtube.ng',
  'youtube.nl',
  'youtube.no',
  'youtube.pe',
  'youtube.ph',
  'youtube.pk',
  'youtube.pl',
  'youtube.pr',
  'youtube.pt',
  'youtube.qa',
  'youtube.ro',
  'youtube.rs',
  'youtube.ru',
  'youtube.sa',
  'youtube.se',
  'youtube.sg',
  'youtube.si',
  'youtube.sk',
  'youtube.sn',
  'youtube.tn',
  'youtube.ua',
  'youtube.ug',
  'youtube.uy',
  'youtube.vn',
  'youtubeeducation.com',
  'youtubemobilesupport.com',
  'ytimg.com'
)


CC_HEADER = """// AUTOGENERATED FILE. DO NOT EDIT.
//
// (Update configs in components/domain_reliability/baked_in_configs and list
// configs in components/domain_reliability/baked_in_configs.gypi instead.)

#include "components/domain_reliability/baked_in_configs.h"

#include <stdlib.h>

namespace domain_reliability {

const char* const kBakedInJsonConfigs[] = {
"""


CC_FOOTER = """  nullptr
};

}  // namespace domain_reliability
"""


def read_json_files_from_gypi(gypi_file):
  with open(gypi_file, 'r') as f:
    gypi_text = f.read()
  json_files = ast.literal_eval(gypi_text)['variables']['baked_in_configs']
  return json_files


def read_json_files_from_file(list_file):
  with open(list_file, 'r') as f:
    list_text = f.read()
  return shlex.split(list_text)


def origin_is_whitelisted(origin):
  if origin.startswith('https://') and origin.endswith('/'):
    domain = origin[8:-1]
  else:
    return False
  return any(domain == e or domain.endswith('.' + e)  for e in DOMAIN_WHITELIST)


def quote_and_wrap_text(text, width=79, prefix='  "', suffix='"'):
  max_length = width - len(prefix) - len(suffix)
  output = prefix
  line_length = 0
  for c in text:
    if c == "\"":
      c = "\\\""
    elif c == "\n":
      c = "\\n"
    elif c == "\\":
      c = "\\\\"
    if line_length + len(c) > max_length:
      output += suffix + "\n" + prefix
      line_length = 0
    output += c
    line_length += len(c)
  output += suffix
  return output


def main():
  parser = optparse.OptionParser(usage="bake_in_configs.py [options]")
  parser.add_option("", "--output", metavar="FILE",
                    help="[Required] Name of the .cc file to write.")

  # For response file reading.
  parser.add_option("", "--file-list", metavar="FILE",
                    help="File containing whitespace separated names of "
                         "the baked in configs files.")

  # For .gypi file reading.
  parser.add_option("", "--gypi-file", metavar="FILE",
                    help=".gypi file containing baked_in_configs variable.")
  parser.add_option("", "--gypi-relative-to", metavar="PATH",
                    help="Directory the baked_in_configs in the --gypi-file"
                         "are relative to.""")

  opts, args = parser.parse_args()

  if not opts.output:
    print("--output argument required", file=sys.stderr)
    return 1

  if opts.gypi_file:
    # .gypi-style input.
    if not opts.gypi_relative_to:
      print("--gypi-relative-to is required with --gypi-file", file=sys.stderr)
      return 1
    json_files = read_json_files_from_gypi(opts.gypi_file)
    json_files = [ os.path.join(opts.gypi_relative_to, f) for f in json_files ]
    json_files = [ os.path.normpath(f) for f in json_files ]
  elif opts.file_list:
    # Regular file list input.
    json_files = read_json_files_from_file(opts.file_list)
  else:
    print("Either --file-list or --gypi-file is required.", file=sys.stderr)
    return 1

  cpp_code = CC_HEADER
  found_invalid_config = False

  for json_file in json_files:
    with open(json_file, 'r') as f:
      json_text = f.read()
    try:
      config = json.loads(json_text)
    except ValueError as e:
      print("%s: error parsing JSON: %s" % (json_file, e), file=sys.stderr)
      found_invalid_config = True
      continue
    if 'origin' not in config:
      print('%s: no origin found' % json_file, file=sys.stderr)
      found_invalid_config = True
      continue
    origin = config['origin']
    if not origin_is_whitelisted(origin):
      print(
          '%s: origin "%s" not in whitelist' % (json_file, origin),
          file=sys.stderr)
      found_invalid_config = True
      continue

    # Re-dump JSON to get a more compact representation.
    dumped_json_text = json.dumps(config, separators=(',', ':'))

    cpp_code += "  // " + json_file + ":\n"
    cpp_code += quote_and_wrap_text(dumped_json_text) + ",\n"
    cpp_code += "\n"

  cpp_code += CC_FOOTER

  if found_invalid_config:
    return 1

  with open(opts.output, 'w') as f:
    f.write(cpp_code)

  return 0


if __name__ == '__main__':
  sys.exit(main())
