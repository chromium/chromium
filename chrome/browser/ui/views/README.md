# Browser Views

This directory contains all [Views](/docs/ui/README.md)-based implementations
of browser UI. in many cases, these implement abstract interfaces in
[chrome/browser/ui/](/chrome/browser/ui/), so that cross-platform code in
Chromium does not need to link directly against Views code or assume that as a
toolkit.

There are a lot of pieces here. As much as possible, please try to add new ones
in their own subdirectories, with specific OWNERS.
