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
  echo "Usage: $0 SRC_OUT_DIR PAGE_SET_DIR OUTPUT_DIR [ADS_HOSTS_CSV]" >&2
  echo "  SRC_OUT_DIR:   Full path to the Chromium build output directory." >&2
  echo "                 Must be a Release build (is_debug=false)." >&2
  echo "  PAGE_SET_DIR:  Directory containing the httparchive page sets" >&2
  echo "                 for the top websites, used by filter_many.sh." >&2
  echo "  OUTPUT_DIR:    Path to save all output and intermediate files." >&2
  echo "  ADS_HOSTS_CSV: (Optional) Path to pre-generated ads hosts CSV." >&2
  echo "                 If not provided, the script will generate it." >&2
}

main() {
  if [[ $# -lt 3 || $# -gt 4 ]]; then
    err "Illegal number of parameters."
    usage
    exit 2
  fi

  # Check for necessary commands
  local cmds=("realpath" "autoninja" "wget" "awk" "uniq" "cut" \
              "head" "grep")
  for cmd in "${cmds[@]}"; do
    command -v "${cmd}" >/dev/null 2>&1 || \
      { err "${cmd} not found. Please install it."; exit 1; }
  done

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
  local ads_hosts_csv_input=""

  # Resolve paths to absolute paths before we cd to the output_dir.
  src_out_dir=$(realpath "$1")
  page_set_dir=$(realpath "$2")
  # Use -m for output_dir, as it might not exist yet.
  output_dir=$(realpath -m "$3")

  if [[ $# -eq 4 ]]; then
    if [[ ! -f "$4" ]]; then
      err "Provided ADS_HOSTS_CSV file not found: $4"
      exit 1
    fi
    ads_hosts_csv_input=$(realpath "$4")
  fi

  # Create output directory and change into it
  mkdir -p "${output_dir}"
  cd "${output_dir}" || { err "Failed to change to output directory: \
                              ${output_dir}"; exit 1; }
  info "Output and intermediate files will be in $(pwd)"

  # If a pre-generated ads hosts CSV is provided, copy it to output_dir
  if [[ -n "${ads_hosts_csv_input}" ]]; then
    info "Using provided ads hosts CSV: ${ads_hosts_csv_input}"
    cp "${ads_hosts_csv_input}" "ads_hosts.csv"
  fi

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

  if [[ -f "ads_hosts.csv" ]]; then
    if [[ ! -s "ads_hosts.csv" ]]; then
      err "ads_hosts.csv is present but empty."
      exit 1
    fi
  else
    local tools_dir="${chromium_src_dir}/components/subresource_filter/tools"
    local generator="${tools_dir}/generate_ads_hosts.sh"
    if [[ ! -f "${generator}" ]]; then
      err "Generator script not found: ${generator}"
      exit 1
    fi
    info "Running generator to generate ads_hosts.csv..."
    bash "${generator}" "ads_hosts.csv"
  fi

  # Validate ads_hosts.csv to ensure it only contains safe 'count,domain' lines
  if grep -q -v -E "^[0-9]+,[a-z0-9.-]+$" ads_hosts.csv; then
    err "ads_hosts.csv contains invalid or unsafe lines!"
    err "Only lines matching the pattern 'count,domain' (e.g.,"
    err "'5,doubleclick.net') are allowed."
    err "First few invalid lines:"
    grep -v -E "^[0-9]+,[a-z0-9.-]+$" ads_hosts.csv | head -n 5 >&2
    exit 1
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

  info "Ensuring domain and host block rules are \$third-party..."
  # Append $third-party (or insert third-party into existing options) for
  # pure domain (||) and host (://) blocklist rules without path slashes.
  python3 -c '
import sys

for line in sys.stdin:
    stripped_line = line.strip()
    if (not stripped_line or
        stripped_line.startswith(("!", "[", "@@")) or
        "##" in stripped_line or "#@" in stripped_line or
        "#?" in stripped_line or "#$" in stripped_line):
        sys.stdout.write(line)
        continue

    rule_parts = stripped_line.split("$", 1)
    pattern = rule_parts[0]
    options_string = rule_parts[1] if len(rule_parts) > 1 else ""

    if pattern.startswith("||"):
        has_path = "/" in pattern[2:]
    elif pattern.startswith("://"):
        has_path = "/" in pattern[3:]
    else:
        sys.stdout.write(line)
        continue

    if has_path:
        sys.stdout.write(line)
        continue

    option_items = (
        [option.strip() for option in options_string.split(",")]
        if options_string else []
    )
    has_party_option = any(
        option.lstrip("~") in ("third-party", "1st-party")
        for option in option_items
    )
    if has_party_option:
        sys.stdout.write(line)
        continue

    newline_sequence = "\r\n" if line.endswith("\r\n") else "\n"
    if options_string:
        updated_options = f"third-party,{options_string}"
    else:
        updated_options = "third-party"
    sys.stdout.write(f"{pattern}${updated_options}{newline_sequence}")
' < candidate_rules.txt > candidate_rules_third.txt

  if [[ ! -s candidate_rules_third.txt ]]; then
    err "Failed to generate candidate_rules_third.txt (empty or missing)."
    exit 1
  fi

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
