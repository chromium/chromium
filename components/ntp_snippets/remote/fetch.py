#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fetches articles from the server.

Examples:
    $ fetch.py                        # unauthenticated, no experiments
    $ fetch.py --short                # abbreviate instead of dumping JSON
    $ fetch.py --signed-in -x3313279  # authenticated, results from Google Now

If getting signed-in results, authenticates with OAuth2 and stores the
credentials at ~/.zineauth.
"""

from __future__ import absolute_import, division, print_function, unicode_literals

import argparse
import base64
import datetime
import json
import os
import textwrap
import oauth2client.client
import oauth2client.file
import oauth2client.tools
import requests
import sys


API_KEY_FILE = os.path.join(
    os.path.dirname(__file__),
    "../../../google_apis/internal/google_chrome_api_keys.h")
API_SCOPE = "https://www.googleapis.com/auth/chrome-content-suggestions"
API_HOSTS = {
    "prod": "https://chromecontentsuggestions-pa.googleapis.com",
    "staging": "https://staging-chromecontentsuggestions-pa.googleapis.com",
    "alpha": "https://alpha-chromecontentsuggestions-pa.sandbox.googleapis.com",
}
API_PATH = "/v1/suggestions/fetch"


def main():
  default_lang = os.environ.get("LANG", "en_US").split(".")[0]

  parser = argparse.ArgumentParser(
      description="fetch articles from server",
      parents=[oauth2client.tools.argparser])
  parser.add_argument("-c", "--component",
                      default="prod", choices=["prod", "staging", "alpha"],
                      help="component to fetch from (default: prod)")
  parser.add_argument("-x", "--experiment", action="append", type=int,
                      help="include an experiment ID")
  parser.add_argument("-l", "--ui-language", default=default_lang,
                      help="language code (default: %s)" % default_lang)
  parser.add_argument("--ip", help="fake IP address")
  parser.add_argument("--api-key", type=str,
                      help="API key to use for unauthenticated requests"
                      " (default: use official key)")
  parser.add_argument("-s", "--signed-in", action="store_true",
                      help="sign in and issue authenticated request")
  parser.add_argument("--client", metavar="ID,SECRET", type=str,
                      help="client project to use for authenticated requests"
                      " (default: use official project ID")
  parser.add_argument("--short", action="store_true",
                      help="print results in abbreviated form")
  args = parser.parse_args()

  r = PostRequest(args)
  j = {}
  try:
    j = r.json()
  except ValueError:
    print(r.text.encode("utf-8"))
    sys.exit(1)
  if j.get("error"):
    print(r.text.encode("utf-8"))
    sys.exit(1)
  if args.short:
    PrintShortResponse(j)
    return
  print(r.text.encode("utf-8"))
  if r.status_code != 200:
    sys.exit(1)


def GetApiKeyFile():
  return API_KEY_FILE


def GetAPIDefs():
  """Parses the internal file with API keys and returns a dict."""
  with open(GetApiKeyFile()) as f:
    lines = f.readlines()
  defs = {}
  next_name = None
  for line in lines:
    if next_name:
      defs[next_name] = json.loads(line)
      next_name = None
    elif line.startswith("#define"):
      try:
        _, name, value = line.split()
      except ValueError:
        continue
      if value == "\\":
        next_name = name
      else:
        defs[name] = json.loads(value)
  return defs


def GetAPIKey():
  return GetAPIDefs()["GOOGLE_API_KEY"]


def GetOAuthClient():
  defs = GetAPIDefs()
  return defs["GOOGLE_CLIENT_ID_MAIN"], defs["GOOGLE_CLIENT_SECRET_MAIN"]


def EncodeExperiments(experiments):
  """Turn a list of experiment IDs into an X-Client-Data header value.

  Encodes all the IDs as a protobuf (tag 1, varint) and base64 encodes the
  result.
  """
  binary = b""
  for exp in experiments:
    binary += b"\x08"
    while True:
      byte = (exp & 0x7f)
      exp >>= 7
      if exp:
        binary += chr(0x80 | byte)
      else:
        binary += chr(byte)
        break
  return base64.b64encode(binary)


def AbbreviateDuration(duration):
  """Turn a datetime.timedelta into a short string like "10h 14m"."""
  w = duration.days // 7
  d = duration.days % 7
  h = duration.seconds // 3600
  m = (duration.seconds % 3600) // 60
  s = duration.seconds % 60
  us = duration.microseconds
  if w:
    return "%dw %dd" % (w, d)
  elif d:
    return "%dd %dh" % (d, h)
  elif h:
    return "%dh %dm" % (h, m)
  elif m:
    return "%dm %ds" % (m, s)
  elif s:
    return "%ds" % s
  elif us:
    return "<1s"
  else:
    return "0s"


def PostRequest(args):
  url = API_HOSTS[args.component] + API_PATH
  headers = {}

  if args.experiment:
    headers["X-Client-Data"] = EncodeExperiments(args.experiment)

  if args.ip is not None:
    headers["X-User-IP"] = args.ip

  if args.signed_in:
    if args.client:
      client_id, client_secret = args.client.split(",")
    else:
      client_id, client_secret = GetOAuthClient()
    Authenticate(args, headers, client_id, client_secret)
  else:
    if args.api_key:
      api_key = args.api_key
    else:
      api_key = GetAPIKey()
    url += "?key=" + api_key

  data = {
    "uiLanguage": args.ui_language,
  }

  return requests.post(url, headers=headers, data=data)


def Authenticate(args, headers, client_id, client_secret):
  storage = oauth2client.file.Storage(os.path.expanduser("~/.zineauth"))
  creds = storage.get()
  if not creds or creds.invalid or creds.access_token_expired:
    flow = oauth2client.client.OAuth2WebServerFlow(
        client_id=client_id, client_secret=client_secret,
        scope=API_SCOPE)
    oauth2client.tools.run_flow(flow, storage, args)
    creds = storage.get()
  creds.apply(headers)


def PrintShortResponse(j):
  now = datetime.datetime.now()
  for category in j["categories"]:
    print("%s: " % category["localizedTitle"])
    for suggestion in category.get("suggestions", []):
      attribution = suggestion["attribution"]
      title = suggestion["title"]
      full_url = suggestion["fullPageUrl"]
      amp_url = suggestion.get("ampUrl")
      creation_time = suggestion["creationTime"]

      if len(title) > 40:
        title = textwrap.wrap(title, 40)[0] + "…"
      creation_time = ParseDateTime(creation_time)
      age = AbbreviateDuration(now - creation_time)

      print("  “%s” (%s, %s ago)" % (title, attribution, age))
      print("    " + (amp_url or full_url))
    if category["allowFetchingMoreResults"]:
      print("  [More]")


def ParseDateTime(creation_time):
  try:
    return datetime.datetime.strptime(creation_time, "%Y-%m-%dT%H:%M:%SZ")
  except ValueError:
    return datetime.datetime.strptime(creation_time, "%Y-%m-%dT%H:%M:%S.%fZ")


if __name__ == "__main__":
  main()
