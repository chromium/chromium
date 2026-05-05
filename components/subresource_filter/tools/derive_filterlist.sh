#!/bin/bash
#
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Generates a smaller filter list from EasyList and EasyPrivacy by filtering
# out the least-frequently-used rules based on a list of subresource requests
# from some top websites.
# Note: This script is a part of the larger filterlist generation process
# described in FILTER_LIST_GENERATION.md in the Chromium source tree.
#
# Dependencies:
# - A Chromium development environment (for autoninja and build output)
# - depot_tools in PATH (for autoninja)
# - wget, curl, gunzip, awk, sed, sort, uniq, cut, head, grep, realpath
# - GNU Parallel (e.g., sudo apt-get install parallel)

# Exit on error, undefined variable, or pipe failure
set -euo pipefail

info() {
  echo "[INFO] $(date +'%Y-%m-%dT%H:%M:%S%z'): $*"
}

err() {
  echo "[ERROR] $(date +'%Y-%m-%dT%H:%M:%S%z'): $*" >&2
}

usage() {
  echo "Usage: $0 SRC_OUT_DIR PAGE_SET_DIR OUTPUT_DIR" >&2
  echo "  SRC_OUT_DIR:  Full path to the Chromium build output directory." >&2
  echo "                Must be a Release build (is_debug=false)." >&2
  echo "  PAGE_SET_DIR: Directory containing the httparchive page sets" >&2
  echo "                for the top websites, used by filter_many.sh." >&2
  echo "  OUTPUT_DIR:   Path to save all output and intermediate files." >&2
}

main() {
  if [[ $# -ne 3 ]]; then
    err "Illegal number of parameters."
    usage
    exit 2
  fi

  # Check for necessary commands
  local cmds=("realpath" "autoninja" "wget" "curl" "gunzip" "awk" "sed" "sort" \
              "uniq" "cut" "head" "grep" "parallel")
  for cmd in "${cmds[@]}"; do
    command -v "${cmd}" >/dev/null 2>&1 || \
      { err "${cmd} not found. Please install it."; exit 1; }
  done

  # Specific check for GNU parallel
  if ! parallel --version 2>/dev/null | grep -iq "gnu parallel"; then
    err "GNU parallel is required, but a different version was found."
    exit 1
  fi

  # Validate input directories before resolving paths
  if [[ ! -d "$1" ]]; then
    err "SRC_OUT_DIR not found: $1"
    exit 1
  fi
  if [[ ! -d "$2" ]]; then
    err "PAGE_SET_DIR not found: $2"
    exit 1
  fi

  local src_out_dir
  local page_set_dir
  local output_dir

  # Resolve paths to absolute paths before we cd to the output_dir.
  src_out_dir=$(realpath "$1")
  page_set_dir=$(realpath "$2")
  # Use -m for output_dir, as it might not exist yet.
  output_dir=$(realpath -m "$3")

  # Create output directory and change into it
  mkdir -p "${output_dir}"
  cd "${output_dir}" || { err "Failed to change to output directory: \
                              ${output_dir}"; exit 1; }
  info "Output and intermediate files will be in $(pwd)"

  local chromium_src_dir
  chromium_src_dir=$(realpath "${src_out_dir}/../../")
  if [[ ! -d "${chromium_src_dir}/components/subresource_filter" ]]; then
    err "Failed to locate Chromium src directory from ${src_out_dir}"
    exit 1
  fi

  info "Compiling subresource_filter_tools..."
  autoninja -C "${src_out_dir}" subresource_filter_tools

  info "Downloading filter lists..."
  wget -O easylist.txt https://easylist.to/easylist/easylist.txt
  wget -O easyprivacy.txt https://easylist.to/easylist/easyprivacy.txt

  info "Downloading top 1000 sites list from CrUX..."
  curl -sL "https://github.com/zakird/crux-top-lists/raw/refs/heads/main/data/global/current.csv.gz" | \
    gunzip -c | \
    awk -F ',' 'NR > 1 && $2 + 0 <= 1000 { print $1 }' > top1000.txt

  info "Scraping ads.txt to find ad tech domains (this may take a while)..."

  # Temporarily disable exit on error and pipefail for the scraping pipeline
  # since some sites don't have ads.txt files.
  set +e
  set +o pipefail

  # 1. Download ads.txt from the given URL.
  # 2. Filter out lines that don't contain a domain.
  # 3. Convert to lowercase.
  # 4. Remove the "ownerdomain" line.
  # 5. Sort and deduplicate the domains.
  fetch_ads_data() {
      local url="$1"
      # Clean the URL (remove trailing slash)
      local target=$(echo "$url" | sed 's/\/$//')

      wget -qO- "$target/ads.txt" 2>/dev/null | \
      grep -E "^[a-zA-Z0-9-]+\." | \
      awk -F, '{print tolower($1)}' | \
      grep -v "^ownerdomain$" | \
      sort -u
  }

  # 2. Export the function for GNU Parallel
  export -f fetch_ads_data

  # 1. Read the top 1000 sites.
  # 2. Run fetch_ads_data in parallel for each site.
  # 3. Count the number of times each domain appears.
  # 4. Filter out domains that appear in fewer than 5 ads.txt files.
  # 5. Output the domains to ads_hosts.csv.
  cat top1000.txt | \
    parallel --timeout 3 --progress -j 32 fetch_ads_data {} | \
    sort | \
    uniq -c | \
    sort -nr | \
    awk '$1 >= 5 {print $1 "," $2}' > ads_hosts.csv

  SCRAPING_EC=$?

  # Restore shell options
  set -e
  set -o pipefail

  if [[ ${SCRAPING_EC} -ne 0 ]]; then
    err "The ads.txt scraping pipeline finished with a non-zero exit code: \
         ${SCRAPING_EC}."
    exit 1
  else
    info "ads.txt scraping pipeline completed without error."
  fi

  if [[ ! -s "ads_hosts.csv" ]]; then
    err "ads_hosts.csv is missing or empty."
    exit 1
  else
    info "ads_hosts.csv generated with $(wc -l < ads_hosts.csv) lines."
  fi

  info "Extracting domain-blocking rules for ad hosts from EasyPrivacy..."
  awk '
      BEGIN { FS="," }
      FNR==NR {
          # File 1: ads_hosts.csv - Build the ads hosts array.
          if ($0 !~ /^[#!]|^$/) {
              hosts[tolower($2)] = 1
          }
          next
      }
      # File 2: easyprivacy.txt - Process the rules.
      {
          # Skip comments, directives, and empty lines
          if ($0 ~ /^[!#]|^$/) {
              next
          }
          # Keep only domain-based block rules: ||domain.example^ or
          # ||domain.example^$third-party AND ensure there are no paths
          # in the rule.
          if ($0 ~ /^\|\|/ && $0 !~ /\// && ($0 ~ /\^$/ || $0 ~ /\^\$third-party$/)) {
              # Extract the domain from the rule by matching the "||" and the
              # domain characters following it.
              if (match($0, /^\|\|[^$^/]+/)) {
                  # Extract the substring, skipping the leading "||"
                  domain = substr($0, RSTART + 2, RLENGTH - 2)

                  # Check if the extracted domain is in our ads hosts list
                  if (hosts[domain] == 1) {
                      print $0
                  }
              }
          }
      }
  ' ads_hosts.csv easyprivacy.txt > easy_privacy_ads_rules.txt

  info "Merging EasyList and extracted EasyPrivacy ad rules..."
  cat easylist.txt easy_privacy_ads_rules.txt | sort -u > candidate_rules.txt

  info "Ensuring all domain block rules are \$third-party..."
  # Convert ||domain.xyz^ rules to ||domain.xyz^$third-party
  # to avoid matching first-party requests on the domain itself.
  awk '
      {
          # Skip comments, allowlist rules (@@), and empty lines.
          if (/^\s*!|^\s*@@|^\s*$/) {
              print
              next
          }
          # If it is a domain rule like ||domain.xyz^ without existing options.
          if (/^\s*\|\|[^/]+\^\s*$/) {
              print $0 "$third-party"
              next
          }
          # Otherwise, print the line as is.
          print
      }
  ' candidate_rules.txt > candidate_rules_third.txt

  local converter="${src_out_dir}/ruleset_converter"
  local indexer="${src_out_dir}/subresource_indexing_tool"
  local filter_tool="${src_out_dir}/subresource_filter_tool"
  local filter_many_script="${chromium_src_dir}/components/subresource_filter/tools/filter_many.sh"

  # AWK script to aggregate multi-line ruleset_converter errors into a summary
  # that contains unique errors sorted by occurrence count.
  #
  # The converter outputs errors in a strict 3-line format:
  # 1. Error message header (e.g., "... ] (error:1) Unknown option...").
  # 2. The literal rule string that failed.
  # 3. A pointer line with a '^' indicating the exact position of the failure.
  #
  # Exits with a failure code to alert the developer if the Chromium log format
  # changes.
  local aggregate_converter_errors_awk='
    BEGIN { state = 0 }

    /^\[.*:ERROR:.*\]/ {
        if (state != 0) {
            print "[FATAL] ruleset_converter log format changed (unexpected ERROR header)." > "/dev/stderr"
            exit 1
        }
        # Strip the timestamp/file prefix and the optional "(error:XX)" code.
        sub(/^\[[^\]]+\][ \t]*(\(error:[0-9]+\)[ \t]*)?/, "")
        sub(/:[ \t]*$/, "") # Clean trailing colons
        error_msg = $0
        state = 1
        next
    }

    state == 1 {
        rule_line = $0
        state = 2
        next
    }

    state == 2 {
        pos = index($0, "^")
        if (pos == 0) {
            print "[FATAL] ruleset_converter log format changed (missing \"^\" pointer)." > "/dev/stderr"
            exit 1
        }

        # Extract the specific option flagged by the pointer.
        opt_name = substr(rule_line, pos)
        sub(/[,=].*/, "", opt_name) # Truncate at the first comma or equals sign
        if (opt_name == "") opt_name = "unknown"

        key = error_msg ": `" opt_name "`"
        if (++counts[key] == 1) examples[key] = rule_line

        state = 0
        next
    }

    # Swallow empty lines left behind by the stripped errors
    /^[ \t]*$/ { next }

    {
        if (state != 0) {
            print "[FATAL] ruleset_converter log format changed (unexpected line during error parsing)." > "/dev/stderr"
            exit 1
        }
        print $0
    }

    END {
        if (state != 0) {
            print "[FATAL] ruleset_converter log format changed (truncated error log)." > "/dev/stderr"
            exit 1
        }

        # Collect unique keys
        for (k in counts) keys[++n] = k

        # Sort keys by occurrence count (descending)
        for (i = 1; i <= n; i++) {
            for (j = i + 1; j <= n; j++) {
                if (counts[keys[j]] > counts[keys[i]]) {
                    tmp = keys[i]; keys[i] = keys[j]; keys[j] = tmp
                }
            }
        }

        # Print formatted summary report
        for (i = 1; i <= n; i++) {
            k = keys[i]
            c = counts[k]
            printf "\n[ERROR] %s (%d occurrence%s)\n", k, c, (c == 1 ? "" : "s")
            printf "Example: %s\n", examples[k]
        }
        if (n > 0) print ""
    }
  '

  info "Converting candidate rules to unindexed format..."
  "${converter}" --input_format=filter-list --output_format=unindexed-ruleset \
    --input_files=candidate_rules_third.txt --output_file=easylist_unindexed 2>&1 | awk \
      "${aggregate_converter_errors_awk}"

  info "Indexing candidate rules..."
  "${indexer}" easylist_unindexed easylist_indexed

  info "Generating smaller filter list using filter_many.sh (this will take several minutes)..."

  # Count the number of gzip files in the page set directory to establish the
  # denominator for our progress status.
  local total_files=$(ls -1 "${page_set_dir}"/*.gz 2>/dev/null | wc -l)

  # filter_many.sh uses the page set from page_set_dir to simulate navigations.
  # The '0' means use as many processes as possible.
  # The awk script filters known noise (gunzip logs) into a dynamic progress
  # status, while preserving actual errors.
  sh "${filter_many_script}" 0 "${page_set_dir}" "${filter_tool}" easylist_indexed 2>&1 > ordered_list.txt | \
    awk -v total="${total_files}" '
      /gunzip -c/ {
          if (total > 0) {
              count++
              percent = (count / total) * 100
              # Use \r to return to the start of the line and \033[K to clear the current line,
              # creating a dynamic progress status that updates in place.
              printf "\r\033[K[INFO] Unpacking page sets: [%d/%d] (%.1f%%)", count, total, percent
              fflush() # Force awk to flush the buffer immediately so the progress status is smooth
          }
          next # Consume the noise log so it does not print normally
      }
      {
          # Pass any non-gunzip output back to stderr.
          # This ensures we do not swallow actual crashes or error logs from filter_many.sh.
          print $0 > "/dev/stderr"
      }
      END {
          # Drop to a new line once complete so subsequent logs do not overwrite the final 100% state.
          if (total > 0 && count > 0) print ""
      }
    '

  info "Extracting top 1000 most frequently hit rules..."
  head -n 1000 ordered_list.txt | cut -d' ' -f2 > smaller_list.txt

  info "Appending all allowlist rules from original lists to be safe..."
  grep ^@@ easylist.txt >> smaller_list.txt
  grep ^@@ easyprivacy.txt >> smaller_list.txt

  info "Appending ever-present rule used for demo/testing purposes..."
  echo "^ad_filterlist_demo_param=1^" >> smaller_list.txt

  info "Creating final sorted, unique list..."
  sort -u smaller_list.txt > final_list.txt

  info "Converting final list to unindexed format..."
  "${converter}" --input_format=filter-list --output_format=unindexed-ruleset \
    --input_files=final_list.txt --output_file=final_list_unindexed 2>&1 | awk \
      "${aggregate_converter_errors_awk}"

  info "Indexing final list for Chromium..."
  "${indexer}" final_list_unindexed final_list_indexed

  info "Script completed. Final indexed ruleset is 'final_list_indexed' in ${output_dir}"
}

main "$@"
