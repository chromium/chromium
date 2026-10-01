#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
A script to verify if a user meets the requirements to become an OWNER of the
//chrome directory in the Chromium repository.

This script checks for:
1.  Minimum number of commits in the //chrome directory.
2.  Minimum number of code reviews in the //chrome directory.
3.  Ownership of at least three subdirectories within //chrome, correctly
    handling `file://` directives in OWNERS files.
"""

import argparse
import subprocess
import sys
from pathlib import Path
from typing import List, Set

# --- Configuration Constants ---
COMMIT_THRESHOLD = 300
REVIEW_THRESHOLD = 500
OWNERS_THRESHOLD = 3
SEARCH_DIRECTORY = "chrome"


class Colors:
  """ANSI color codes for beautiful terminal output."""
  GREEN = '\033[92m'
  YELLOW = '\033[93m'
  RED = '\033[91m'
  BOLD = '\033[1m'
  END = '\033[0m'


def _run_git_command(command: List[str]) -> str:
  """
    Executes a Git command and returns its stripped stdout.
    Handles potential errors by printing a message and exiting.
    """
  try:
    process = subprocess.run(command,
                             capture_output=True,
                             text=True,
                             check=True,
                             encoding='utf-8')
    return process.stdout.strip()
  except FileNotFoundError:
    print(
        f"{Colors.RED}Error: 'git' command not found. "
        f"Is Git installed and in your PATH?{Colors.END}",
        file=sys.stderr)
    sys.exit(1)
  except subprocess.CalledProcessError as e:
    # A non-zero exit code often means not in a repo.
    error_message = e.stderr.strip()
    if "not a git repository" in error_message:
      print(
          f"{Colors.RED}Error: This script must be run from within "
          f"the Chromium git repository.{Colors.END}",
          file=sys.stderr)
    else:
      print(f"{Colors.RED}Git command failed:\n{error_message}"
            f"{Colors.END}",
            file=sys.stderr)
    sys.exit(1)


def get_commit_count(email: str, directory: str) -> int:
  """Counts commits by a given author in a specific directory."""
  print(f"[*] Checking commit count for {Colors.BOLD}{email}{Colors.END}...")
  command = [
      "git", "rev-list", "--count", f"--author={email}", "HEAD", "--", directory
  ]
  output = _run_git_command(command)
  return int(output) if output.isdigit() else 0


def get_review_count(email: str, directory: str) -> int:
  """Counts reviews by a given user in a specific directory."""
  print(f"[*] Checking review count for {Colors.BOLD}{email}{Colors.END}...")
  # We run the git log command and count the resulting lines within Python.
  # This avoids a shell pipe and is more robust and platform-independent.
  command = [
      "git", "log", f"--grep=Reviewed-by:.*<{email}>", "-E", "-i", "--oneline",
      "--", directory
  ]
  output = _run_git_command(command)
  # An empty output from git log should result in 0 reviews.
  return len(output.splitlines()) if output else 0


def _is_user_in_owner_file_recursively(owner_file: Path, email_lower: str,
                                       visited_files: Set[Path]) -> bool:
  """
    Recursively checks if an email is in an OWNERS file, following `file://`.

    Args:
        owner_file: The path to the OWNERS file to check.
        email_lower: The lowercase email address to search for.
        visited_files: A set of paths already checked to prevent infinite loops.

    Returns:
        True if the user is found, False otherwise.
    """
  if owner_file in visited_files or not owner_file.is_file():
    return False

  visited_files.add(owner_file)

  with owner_file.open('r', encoding='utf-8') as f:
    for line in f:
      stripped_line = line.strip()
      # Ignore comments and empty lines
      if not stripped_line or stripped_line.startswith('#'):
        continue

      # Direct email match
      if email_lower in stripped_line.lower():
        return True

      # file:// directive match
      if stripped_line.lower().startswith('file://'):
        # Assumes the path is relative to the repository root.
        referenced_path = Path(stripped_line[len('file://'):])
        if _is_user_in_owner_file_recursively(referenced_path, email_lower,
                                              visited_files):
          return True
  return False


def find_owned_directories(email: str, directory: str) -> List[Path]:
  """
    Finds all OWNERS files within a directory that list the user as an owner.
    """
  print(
      f"[*] Searching for OWNERS files for {Colors.BOLD}{email}{Colors.END}...")
  owned_files = []
  root_dir = Path(directory)
  email_lower = email.lower()

  if not root_dir.is_dir():
    print(f"{Colors.YELLOW}Warning: Directory '{directory}' not "
          f"found.{Colors.END}",
          file=sys.stderr)
    return []

  for owner_file in root_dir.rglob("OWNERS"):
    # We start each top-level check with a fresh set of visited files.
    if _is_user_in_owner_file_recursively(owner_file,
                                          email_lower,
                                          visited_files=set()):
      owned_files.append(owner_file)

  return owned_files


def _print_status(label: str, count: int, threshold: int):
  """Prints a formatted status line with a pass/fail indicator."""
  if count >= threshold:
    status = f"{Colors.GREEN}PASS{Colors.END}"
  else:
    status = f"{Colors.RED}FAIL{Colors.END}"
  print(f"  {label:<18} {Colors.BOLD}{count}{Colors.END} "
        f"(Threshold: {threshold}) [{status}]")


def main():
  """Main function to parse arguments and run the checks."""
  parser = argparse.ArgumentParser(
      description="Check Chromium OWNERS requirements for a user.")
  parser.add_argument(
      "email",
      help="The email address of the user to check (e.g., 'user@chromium.org')."
  )
  args = parser.parse_args()
  email = args.email

  print("-" * 60)
  print(f"Checking requirements for: "
        f"{Colors.BOLD}{Colors.YELLOW}{email}{Colors.END}")
  print("-" * 60)

  # --- Perform Checks ---
  commit_count = get_commit_count(email, SEARCH_DIRECTORY)
  review_count = get_review_count(email, SEARCH_DIRECTORY)
  owned_files = find_owned_directories(email, SEARCH_DIRECTORY)
  owners_count = len(owned_files)

  # --- Print Summary ---
  print("\n" + "-" * 60)
  print(f"{Colors.BOLD}SUMMARY{Colors.END}")
  print("-" * 60)

  _print_status("Commit Count:", commit_count, COMMIT_THRESHOLD)
  _print_status("Review Count:", review_count, REVIEW_THRESHOLD)
  _print_status("OWNERS Files:", owners_count, OWNERS_THRESHOLD)

  if owners_count > 0:
    print(f"\n{Colors.BOLD}Found in {owners_count} OWNERS file(s):{Colors.END}")
    # Sort by directory depth to list more top-level directories first,
    # using the path itself as a tie-breaker for alphabetical sorting.
    sorted_paths = sorted(owned_files, key=lambda p: (len(p.parts), p))
    for path in sorted_paths:
      print(f"  - {Colors.GREEN}{path}{Colors.END}")

  print("-" * 60)


if __name__ == "__main__":
  main()
