#!/bin/sh

# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This takes a directory of gzip files that contain all of the url requests
# from the HTTP Archive and runs them against the given filter list, showing
# how many times each filter rule matched in descending order. This script
# is part of the process of building a small filter list, as documented
# in components/subresource_filter/FILTER_LIST_GENERATION.md.

# Example usage, from the directory that contains the gzip files:
# bash filter_many.sh 8 . ~/chromium/src/out/Release/subresource_filter_tool \
#   easylist_indexed > sorted_list

# The number of processes you want to run in parallel. 8 is reasonable for a
# typical machine. 80 is good for a powerful workstation. If 0 is specified,
# this script uses as many as possible.
PROCESS_COUNT=$1

# The path to the directory that contains gzip files of resource requests from
# http archive.
GZIP_PATH=$2

# The path to the filter_tool binary.
FILTER_TOOL=$3

# The path to the indexed easylist file.
EASYLIST=$4

# Create temporary directory.
TEMP_DIR=$(mktemp -d)

# For each gzip file:
ls $GZIP_PATH/*.gz |

# In parallel, unzip the file and count the number of times each rule matches.
# The results are saved to independent temporary files to ensure that writes
# aren't interleaved mid-rule.
xargs -t -I {} -P $PROCESS_COUNT \
  sh -c "gunzip -c {} | \
         $FILTER_TOOL --ruleset=$EASYLIST match_rules \
         > \$(mktemp $TEMP_DIR/output.XXXXXXXXXX)"

# Aggregate the results from those files.
cat $TEMP_DIR/output.* |

# Sort the results by filter rule.
sort -k 2 |

# Combine the matches for the same rule.
awk 'NR>1 && rule!=$2 {print count,rule; count=0} {count+=$1} {rule=$2} \
  END {print count,rule}' |

# Sort the output in descending order by match count.
sort -n -r

# Delete the temporary folder.
rm -rf $TEMP_DIR
