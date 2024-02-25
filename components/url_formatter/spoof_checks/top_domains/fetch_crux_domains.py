#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

r"""Script to fetch top domain list from public CrUX data for use in lookalikes.

Chrome User Experience Report (CrUX) publishes a list of popular domains on
BigQuery (https://developer.chrome.com/docs/crux/bigquery/). This list contains
hostnames bucketed according to their popularity scores. For example, top 1K
domains get assigned a rank of 1000, top 10K domains get assigned a rank of 10K
and so on.

This script queries the monthly published table, extracts the hostnames and
ranks, de-duplicates the hostnames and writes the result in the format of
`domains.list`.

The Chrome build generates skeleton strings and other variables from this list.
These are embedded into the Chrome binary and used for lookalike URL checks.

Requirements:
1. Install cloud SDK (https://cloud.google.com/sdk/docs/install)

2. Get credentials:
  `gcloud auth application-default login`

3. Install Bigquery library
  `pip3 install --user --upgrade google-cloud-bigquery`

4. If asked, set a project:
  `gcloud auth application-default set-quota-project <your-project-name>`
  `gcloud config set project <your-project-name>`

Usage:
```
# 1. Fetch the domains:
components/url_formatter/spoof_checks/top_domains/fetch_crux_domains.py > \
    components/url_formatter/spoof_checks/top_domains/domains.list

# 2. Generate skeletons:
out/Release/make_top_domain_skeletons

# 3. Rebuild (this is needed to generate top domain variables):
autoninja -C out/Release

# 4. Create a CL to upload the new domains.list and domains.skeletons files.
```
"""
import argparse
import sys

from google.cloud import bigquery


def main():
  parser = argparse.ArgumentParser(
      description="Fetch top domain list from CrUX.")
  parser.add_argument(
      "--table",
      default="chrome-ux-report.all.202309",
      help="Monthly table name",
  )
  args = parser.parse_args()

  client = bigquery.Client()

  query = f"""
        SELECT
          NET.REG_DOMAIN(origin) AS hostname,
          experimental.popularity.rank AS rank
        FROM {args.table}
        WHERE experimental.popularity.rank <= 10000
        GROUP BY hostname, rank
        ORDER BY rank ASC, hostname ASC;
    """
  results = client.query(query)

  top_1k = set([
      # These domains don't appear in CrUX list because of redirects.
      # We add them here to improve our coverage.
      "fb.com",
      "gmail.com",
      "hotmail.com",
  ])
  top_10k = set([])

  for row in results:
    hostname = row["hostname"]
    rank = int(row["rank"])
    if rank <= 1000:
      top_1k.add(hostname)
    elif hostname not in top_1k:
      top_10k.add(hostname)

  print(
      f"# This list was generated from {args.table} by fetch_crux_domains.py.")

  for hostname in sorted(list(top_1k)):
    print(hostname)

  print("\n###END_TOP_BUCKET###\n")

  for hostname in sorted(list(top_10k)):
    print(hostname)


if __name__ == "__main__":
  sys.exit(main())
