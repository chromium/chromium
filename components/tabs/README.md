# Tabs

This directory contains the data structures, interfaces, and in-memory storage
for cross platform tab code. As of March 2025 the desktop tab collection code is
being migrated to this directory to be shared with other platforms.

## Tab Collections

`TabCollection`s are n-ary tree data structures that host a mix of tabs and other
tab collections. These collections represent logical groupings of tabs such as
pinned tabs, tab groups, etc. Tabs in tab collections are represented by
platform agnostic `TabInterface`.

The goal of tab collections is to replace the legacy list-based approach to
storing tabs which was error-prone and could result in various bugs such as
noncontiguous tabs being in a tab group, pinned tabs being in tab groups, etc.
