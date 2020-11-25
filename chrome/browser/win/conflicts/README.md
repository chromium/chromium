## Overview

This directory contains all the code related to the blocking of third-party
DLLs.

It has 3 main roles:
1. Detect all DLLs that loads in the browser and renderer processes.
2. Match third-party DLLs to an installed application on the user's computer and
   warn the user via chrome://settings/incompatibleApplications
2. Build the module blocklist cache that allows the chrome_elf hook to block
   non-allowlisted DLLs.
