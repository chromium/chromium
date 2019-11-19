# Chrome Cleanup Tool

This directory contains the source code for the Chrome Cleanup Tool, a
standalone application distributed to Chrome users to find and remove Unwanted
Software (UwS).

The tool and its tests are Windows-only.

## Integration with Chrome

The application is distributed in two versions:

1.  A Chrome component named the Software Reporter that finds UwS but does not
    have the ability to delete anything.
2.  A separate download named Chrome Cleanup Tool that both finds and removes UwS.

The Software Reporter runs in the background and, if it finds UwS that can be
removed, reports this to Chrome. Chrome then downloads the full Cleanup Tool
and shows a prompt on the settings page asking the user for permission to
remove the UwS.

This directory contains the source for both.

Code in Chromium that deals with the Software Reporter Tool and Chrome Cleanup
Tool includes:

*   [Software Reporter component updater](/chrome/browser/component_updater)
    (files `sw_reporter_installer_win*`)
*   [Chrome Cleanup Tool fetcher and launcher](/chrome/browser/safe_browsing/chrome_cleaner)
*   [Settings page resources](/chrome/browser/resources/settings/chrome_cleanup_page)
*   [Settings page user interaction handlers](/chrome/browser/ui/webui/settings)
    (files `chrome_cleanup_handler.*`)
*   [UI for modal dialogs](/chrome/browser/ui/views) (files `chrome_cleaner_*`)
*   [Shared constants and IPC interfaces](/components/chrome_cleaner/public) -
    These are used for communication between Chrome and the Software Reporter /
    Chrome Cleanup Tool, so both this directory and the other Chromium
    directories above have dependencies on them.

## Internal Resources

If |is_official_chrome_cleaner_build| is set in GN, the build looks for
internal resources in the chrome_cleaner/internal directory. These resources
are not open source so are only available internally to Google. They include
the licensed scanning engine used to find real-world UwS.

Otherwise the build will link to the test scanning engine in
`chrome/chrome_cleaner/engines/target/test_engine_delegate.cc` which only
detects test files. This is the default when building on the Chromium
buildbots.

To ship a non-test version of the tool, implement EngineDelegate to wrap an
engine that can detect and remove UwS. The engine will be run inside a sandbox
with low privileges. To perform operations like opening file handles, scanning
process memory, and deleting files, the engine will need to call the callbacks
passed to EngineDelegate as |privileged_file_calls|, |privileged_scan_calls|
and |privileged_removal_calls|.

## Status

Code complete. Some tests are still in the Google internal repository.

The unit tests (`chrome_cleaner_unittests.exe`) are built and executed on the
Chromium buildbots. The final binaries (`chrome_cleanup_tool.exe` and
`software_reporter_tool.exe`) are also built because chrome_cleaner_unittests
has an artificial dependency on them, but nothing currently executes them.

[TODO(crbug.com/949669)](https://crbug.com/949669): add an integration test
that builds and runs the binaries, and remove the artificial dependency.

## Contact

joenotcharles@chromium.org
